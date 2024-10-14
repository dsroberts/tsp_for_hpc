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

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define CGROUP_CPUSET_PATH_PREFIX "/sys/fs/cgroup/cpuset"
#define CPUSET_FILE "/cpuset.cpus"
#define SEMAPHORE_FILE_TEMPLATE "/tmp/tsp_hpc_is_waiting"
#define BASE_WAIT_PERIOD 2
#define JITTER_MS 250

class Semaphore_File
{
public:
    Semaphore_File()
    {
        fn = strdup(SEMAPHORE_FILE_TEMPLATE ".XXXXXX");
        if ((fd = mkstemp(fn)) == -1)
        {
            throw std::runtime_error("Unable to create semaphore file ret=" + std::to_string(fd) + " errno=" + std::to_string(errno));
        }
        unlink(fn);
    }
    ~Semaphore_File()
    {
        close(fd);
        free(fn);
    }

private:
    char *fn;
    int fd;
};

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
                if (std::filesystem::read_symlink(entry.path() / "exe") == my_path)
                {
                    out.push_back(std::stoul(entry.path().filename()));
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
        for (const auto &entry : std::filesystem::directory_iterator("/proc/" + std::to_string(pid) + "/fd"))
        {
            try
            {
                if (std::filesystem::read_symlink(entry).string().find(SEMAPHORE_FILE_TEMPLATE) != std::string::npos)
                {
                    // Semaphore present, ignore
                    return out;
                }
            }
            catch (std::filesystem::filesystem_error &e)
            {
                // Symlink was deleted before we read it, so
                // we actually do care about this process's
                // affinity
            }
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

class Run_cmd
{
public:
    std::vector<char *> proc_to_run;
    bool is_openmpi;
    Run_cmd(char *cmdline[], int start, int end)
    {
        for (int i = start; i < end; i++)
        {
            proc_to_run.push_back(cmdline[i]);
        }
        is_openmpi = check_mpi();
    }
    ~Run_cmd()
    {
        if (rf_copy != nullptr)
        {
            free(rf_copy);
        }
        if (dash_rf != nullptr)
        {
            free(dash_rf);
        }
        if (is_openmpi)
        {
            if (!rf_name.empty())
            {
                std::filesystem::remove(rf_name);
            }
        }
    }

    void add_rankfile(std::vector<uint32_t> procs, uint32_t nslots)
    {
        rf_name = make_rankfile(procs, nslots);
        const char *rf_str = rf_name.c_str();
        rf_copy = strdup(rf_str);
        dash_rf = strdup("-rf");
        proc_to_run.insert(proc_to_run.begin() + 1, rf_copy);
        proc_to_run.insert(proc_to_run.begin() + 1, dash_rf);
        proc_to_run.push_back(nullptr);
    }

private:
    char *rf_copy = nullptr;
    char *dash_rf = nullptr;
    std::filesystem::path rf_name;
    std::filesystem::path make_rankfile(std::vector<uint32_t> procs, uint32_t nslots)
    {
        std::filesystem::path rank_file = "/tmp/" + std::to_string(getpid()) + "_rankfile.txt";
        std::ofstream rf_stream(rank_file);
        if (rf_stream.is_open())
        {
            for (uint32_t i; i < nslots; i++)
            {
                rf_stream << "rank " + std::to_string(i) + "=localhost slot=" + std::to_string(procs[i]) << std::endl;
            }
        }
        rf_stream.close();
        return rank_file;
    }
    bool check_mpi()
    {
        std::string prog_name(proc_to_run[0]);
        if (prog_name == "mpirun" || prog_name == "mpiexec")
        {
            // OpenMPI does not respect parent process binding,
            // so we need to check if we're attempting to run
            // OpenMPI, and if we are, we need to construct a
            // rankfile and add it to the arguments. Note that
            // this will explode if you're attempting anything
            // other than by-core binding and mapping
            int pipefd[2];
            pipe(pipefd);
            int fork_pid;
            std::string mpi_version_output;
            if (0 == (fork_pid = fork()))
            {
                close(pipefd[0]);
                dup2(pipefd[1], 1);
                close(pipefd[1]);
                execlp(prog_name.c_str(), prog_name.c_str(), "--version", nullptr);
            }
            else
            {
                char buffer[1024];
                close(pipefd[1]);
                while (read(pipefd[0], buffer, sizeof(buffer)) != 0)
                {
                    mpi_version_output.append(buffer);
                }
                close(pipefd[0]);
                if (waitpid(fork_pid, nullptr, 0) == -1)
                {
                    throw std::runtime_error("Error watiting for mpirun test process");
                }
            }
            if (mpi_version_output.find("Open MPI") != std::string::npos ||
                mpi_version_output.find("OpenRTE") != std::string::npos)
            {
                // OpenMPI detected...
                return true;
            }
        }
        return false;
    }
};

class Jitter
{
public:
    Jitter(int limit)
    {
        std::random_device dev;
        rng = std::mt19937(dev());
        dist = std::uniform_int_distribution<int>(-abs(limit), abs(limit));
    }
    auto get()
    {
        return dist(rng);
    }
    ~Jitter() {}

private:
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;
};

int main(int argc, char *argv[])
{
    Semaphore_File *sf = new Semaphore_File();
    Jitter jitter(JITTER_MS);
    // Parse args: only take the ones we use for now
    int c;
    uint32_t nslots(1);
    bool disappear_output = false;
    bool do_fork = true;

    while ((c = getopt(argc, argv, "+nfN:")) != -1)
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
        }
    }

    if (do_fork)
    {
        pid_t main_fork_pid;
        main_fork_pid = fork();
        if (main_fork_pid == -1)
        {
            throw std::runtime_error("Unable to fork when forking requested");
        }
        if (main_fork_pid != 0)
        {
            // We're done here
            exit(0);
        }
    }

    Run_cmd cmd(argv, optind, argc);
    Tsp_Proc me(nslots);

    cpu_set_t mask;
    CPU_ZERO(&mask);

    while (!me.allowed_to_run())
    {
        std::this_thread::sleep_for(std::chrono::seconds(BASE_WAIT_PERIOD) + std::chrono::milliseconds(jitter.get()));
        me.refresh_allowed_cores();
    }
    delete sf;
    for (uint32_t i = 0; i < nslots; i++)
    {
        CPU_SET(me.allowed_cores[i], &mask);
    }
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1)
    {
        throw std::runtime_error("Unable to set CPU affinity");
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
            int fd;
            if (disappear_output)
            {
                fd = open("/dev/null", O_WRONLY | O_CREAT, 0666);
                dup2(fd, 1);
                dup2(fd, 2);
            }
            if (cmd.is_openmpi)
            {
                setenv("OMPI_MCA_rmaps_base_mapping_policy", "", 1);
            }
            ret = execvp(cmd.proc_to_run[0], &cmd.proc_to_run[0]);
            if (disappear_output)
            {
                close(fd);
            }

            if (ret != 0)
            {
                throw std::runtime_error("Error: could not exec " + std::string(cmd.proc_to_run[0]) + ". ret=" + std::to_string(fork_pid) + "errno=" + std::to_string(errno));
            }
        }
        if (waited_on_pid == -1)
        {
            throw std::runtime_error("Error: could not fork init subprocess. ret=" + std::to_string(fork_pid) + "errno=" + std::to_string(errno));
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
        exit(WEXITSTATUS(child_stat));
    }
    if (fork_pid == -1)
    {
        throw std::runtime_error("Error: could not fork into new pid namespace. ret=" + std::to_string(fork_pid) + "errno=" + std::to_string(errno));
    }

    pid_t ret_pid = waitpid(fork_pid, &fork_stat, 0);
    if (ret_pid == -1)
    {
        throw std::runtime_error("Error: failed to wait for forked process. errno=" + std::to_string(errno));
    }

    // Exit with status of forked process.
    exit(WEXITSTATUS(fork_stat));
}