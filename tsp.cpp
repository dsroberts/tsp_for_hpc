#include <cstdlib>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <iterator>
#include <chrono>
#include <thread>
#include <ranges>
#include <cstring>
#include <random>
#include <iomanip>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "functions.hpp"
#include "jitter.hpp"
#include "semaphore.hpp"
#include "run_cmd.hpp"
#include "status_manager.hpp"

#define CGROUP_CPUSET_PATH_PREFIX "/sys/fs/cgroup/cpuset"
#define CPUSET_FILE "/cpuset.cpus"
#define BASE_WAIT_PERIOD 2
#define OUTPUT_FILE_TEMPLATE "/tsp.o"
#define ERROR_FILE_TEMPLATE "/tsp.e"


class Tsp_Proc
{
public:
    pid_t pid;
    std::vector<uint32_t> allowed_cores;

    Tsp_Proc(uint32_t nslots)
    {
        this->nslots = nslots;
        pid = getpid();
        my_path = std::filesystem::read_symlink("/proc/self/exe");
        // Open cgroups file
        cpuset_from_cgroup = get_cgroup();
        if (nslots > cpuset_from_cgroup.size())
        {
            throw std::runtime_error("More slots requested than available on the system, this process can never run.");
        }
        refresh_allowed_cores();
    };

    bool allowed_to_run()
    {
        return allowed_cores.size() > nslots;
    }

    void refresh_allowed_cores()
    {
        auto sibling_pids = get_siblings();
        std::vector<uint32_t> siblings_affinity;
        for (auto i : sibling_pids)
        {
            auto tmp = get_sibling_affinity(i);
            siblings_affinity.insert(siblings_affinity.end(), tmp.begin(), tmp.end());
        }
        std::sort(siblings_affinity.begin(), siblings_affinity.end());
        allowed_cores.clear();
        std::set_difference(cpuset_from_cgroup.begin(), cpuset_from_cgroup.end(),
                            siblings_affinity.begin(), siblings_affinity.end(),
                            std::inserter(allowed_cores, allowed_cores.begin()));
    }

    ~Tsp_Proc() {};

private:
    std::vector<uint32_t> cpuset_from_cgroup;
    std::filesystem::path my_path;
    uint32_t nslots;

    std::vector<pid_t> get_siblings()
    {
        std::vector<pid_t> out;
        std::vector<std::string> skip_paths = {std::to_string(pid), "self", "thread-self"};
        // Find all the other versions of this application running
        for (const auto &entry : std::filesystem::directory_iterator("/proc"))
        {
            if (std::find(skip_paths.begin(), skip_paths.end(), entry.path().filename()) != skip_paths.end())
            {
                continue;
            }
            if (std::filesystem::exists(entry.path() / "exe"))
            {
                try
                {
                    if (std::filesystem::read_symlink(entry.path() / "exe") == my_path)
                    {
                        out.push_back(std::stoul(entry.path().filename()));
                    }
                }
                catch (std::filesystem::filesystem_error &e)
                {
                    // process went away
                    continue;
                }
            }
        }
        return out;
    };

    std::vector<std::uint32_t> parse_cpuset_range(std::string in)
    {
        std::stringstream ss1(in);
        std::string token;
        std::vector<std::uint32_t> out;
        while (std::getline(ss1, token, ','))
        {
            if (token.find('-') == std::string::npos)
            {
                out.push_back(std::stoul(token));
            }
            else
            {
                std::stringstream ss2(token);
                std::string starts, ends;
                std::getline(ss2, starts, '-');
                std::getline(ss2, ends, '-');
                std::vector<std::uint32_t> tmp(std::stoul(ends) - std::stoul(starts) + 1);
                std::iota(tmp.begin(), tmp.end(), std::stoul(starts));
                out.insert(out.end(), tmp.begin(), tmp.end());
            }
        }
        return out;
    };

    std::vector<uint32_t> get_sibling_affinity(pid_t pid)
    {
        std::vector<uint32_t> out;
        cpu_set_t mask;
        // Just return an empty vector if the semaphore file is present
        try
        {
            for (const auto &entry : std::filesystem::directory_iterator("/proc/" + std::to_string(pid) + "/fd"))
                if (std::filesystem::read_symlink(entry).string().find(SEMAPHORE_FILE_TEMPLATE) != std::string::npos)
                {
                    // Semaphore present, ignore
                    return out;
                }
        }

        catch (std::filesystem::filesystem_error &e)
        {
            // Process went away
            return out;
        }
        if (sched_getaffinity(pid, sizeof(mask), &mask) == -1)
        {
            // Process may have been killed - so it isn't taking
            // resources any more
            return out;
        }
        for (const auto &i : cpuset_from_cgroup)
        {
            if (CPU_ISSET(i, &mask))
            {
                out.push_back(i);
            }
        }
        return out;
    };

    std::vector<uint32_t> get_cgroup()
    {
        std::filesystem::path cgroup_fn(std::string("/proc/" + std::to_string(pid) + "/cgroup"));
        if (!std::filesystem::exists(cgroup_fn))
        {
            throw std::runtime_error("Cgroup file for process " + std::to_string(pid) + " not found");
        }
        std::string line;
        std::filesystem::path cpuset_path;
        // get cpuset path
        std::ifstream cgroup_file(cgroup_fn);
        if (cgroup_file.is_open())
        {
            while (std::getline(cgroup_file, line))
            {
                std::vector<std::string> seglist;
                std::string segment;
                std::stringstream ss(line);
                while (std::getline(ss, segment, ':'))
                {
                    seglist.push_back(segment);
                };
                if (seglist[1] == "cpuset")
                {
                    cpuset_path = CGROUP_CPUSET_PATH_PREFIX;
                    cpuset_path += seglist[2];
                    cpuset_path += CPUSET_FILE;
                }
                if (!cpuset_path.empty())
                {
                    break;
                }
            }
            cgroup_file.close();
        }
        else
        {
            throw std::runtime_error("Unable to open cgroup file " + cgroup_fn.string());
        }
        // read cpuset file
        std::ifstream cpuset_file(cpuset_path);
        if (cpuset_file.is_open())
        {
            std::getline(cpuset_file, line);
            return parse_cpuset_range(line);
        }
        else
        {
            throw std::runtime_error("Unable to open cpuset file " + cpuset_path.string());
        }
    }
};

class Output_handler
{
public:
    int stdout_fd = -1;
    int stderr_fd = -1;
    Output_handler(bool disappear, bool separate_stderr, const char *jobid)
    {
        if (disappear)
        {
            stdout_fd = open("/dev/null", O_WRONLY | O_CREAT, 0666);
            stderr_fd = open("/dev/null", O_WRONLY | O_CREAT, 0666);
        }
        else
        {
            asprintf(&stdout_fn, "%s" OUTPUT_FILE_TEMPLATE "%s", get_tmp(), jobid);
            stdout_fd = open(stdout_fn, O_WRONLY | O_CREAT, 0600);
            dup2(stdout_fd, 1);
            if (separate_stderr)
            {
                asprintf(&stderr_fn, "%s" ERROR_FILE_TEMPLATE "%s", get_tmp(), jobid);
                dup2(stderr_fd, 2);
                stderr_fd = open(stderr_fn, O_WRONLY | O_CREAT, 0600);
            }
            else
            {
                dup2(stdout_fd, 2);
            }
        }
    }
    ~Output_handler()
    {
        if (stdout_fn != nullptr)
        {
            free(stdout_fn);
        }
        if (stderr_fn != nullptr)
        {
            free(stderr_fn);
        }
        if (stdout_fd != -1)
        {
            close(stdout_fd);
        }
        if (stderr_fd != -1)
        {
            close(stderr_fd);
        }
    }

private:
    char *stdout_fn = nullptr;
    char *stderr_fn = nullptr;
};

int main(int argc, char *argv[])
{
    tsp::Semaphore_File *sf = new tsp::Semaphore_File();
    // Parse args: only take the ones we use for now
    int c;
    uint32_t nslots = 1;
    bool disappear_output = false;
    bool do_fork = true;
    bool separate_stderr = false;

    if (argc == 1)
    {
        // List all jobs we're aware of
        std::cout << "ID   State      Output               E-Level  Time         Command" << std::endl;
        for (const auto &entry : std::filesystem::directory_iterator(get_tmp()))
        {
            if (entry.path().filename().string().find(STATUS_FILE_TEMPLATE) != std::string::npos)
            {
                std::ifstream stat_file(entry.path());

            }
        }
    }

    while ((c = getopt(argc, argv, "+nfN:E")) != -1)
    {
        switch (c)
        {
        case 'n':
            // Pipe stdout and stderr of forked process to /dev/null
            disappear_output = true;
            break;
        case 'f':
            do_fork = false;
            break;
        case 'N':
            nslots = std::stoul(optarg);
            break;
        case 'E':
            separate_stderr = true;
            break;
        }
    }

    if (do_fork)
    {
        pid_t main_fork_pid;
        main_fork_pid = fork();
        if (main_fork_pid == -1)
        {
            die_with_err("Unable to fork when forking requested", main_fork_pid);
        }
        if (main_fork_pid != 0)
        {
            // We're done here
            return 0;
        }
    }

    tsp::Jitter jitter(JITTER_MS);
    std::chrono::duration sleep(std::chrono::milliseconds(JITTER_MS + jitter.get()));
    std::this_thread::sleep_for(sleep);

    tsp::Run_cmd cmd(argv, optind, argc);
    tsp::Status_Manager stat(cmd, nslots);
    Tsp_Proc me(nslots);

    cpu_set_t mask;
    CPU_ZERO(&mask);

    while (!me.allowed_to_run())
    {
        std::this_thread::sleep_for(std::chrono::seconds(BASE_WAIT_PERIOD) + std::chrono::milliseconds(jitter.get()));
        me.refresh_allowed_cores();
    }
    stat.job_start();
    delete sf;
    for (uint32_t i = 0; i < nslots; i++)
    {
        CPU_SET(me.allowed_cores[i], &mask);
    }
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1)
    {
        die_with_err("Unable to set CPU affinity", -1);
    }
    if (cmd.is_openmpi)
    {
        cmd.add_rankfile(me.allowed_cores, nslots);
    }

    pid_t fork_pid;
    int fork_stat;
    // exec & monitor here.
    if (0 == (fork_pid = fork()))
    {

        // We are now init, so fork again, and wait in a loop until it returns ECHILD
        int child_stat;
        int ret;
        pid_t waited_on_pid;
        if (0 == (waited_on_pid = fork()))
        {
            Output_handler *handler = new Output_handler(disappear_output, separate_stderr, stat.jobid.c_str());
            if (cmd.is_openmpi)
            {
                setenv("OMPI_MCA_rmaps_base_mapping_policy", "", 1);
                setenv("OMPI_MCA_rmaps_rank_file_physical", "true", 1);
            }
            ret = execvp(cmd.proc_to_run[0], &cmd.proc_to_run[0]);
            delete handler;

            if (ret != 0)
            {
                die_with_err("Error: could not exec " + std::string(cmd.proc_to_run[0]), ret);
            }
        }
        if (waited_on_pid == -1)
        {
            die_with_err("Error: could not fork init subprocess", waited_on_pid);
        }
        for (;;)
        {
            pid_t ret_pid = waitpid(-1, &child_stat, 0);
            if (ret_pid < 0)
            {
                if (errno == ECHILD)
                {
                    break;
                }
            }
        }
        return WEXITSTATUS(child_stat);
    }
    if (fork_pid == -1)
    {
        die_with_err("Error: could not fork into new pid namespace", fork_pid);
    }

    pid_t ret_pid = waitpid(fork_pid, &fork_stat, 0);
    if (ret_pid == -1)
    {
        die_with_err("Error: failed to wait for forked process", ret_pid);
    }

    // Exit with status of forked process.
    stat.job_end(WEXITSTATUS(fork_stat));
    return WEXITSTATUS(fork_stat);
}