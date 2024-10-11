#include <cstdlib>
#include <vector>
#include <iostream>
#include <fstream>
#include <format>
#include <string>
#include <filesystem>
#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <iterator>
#include <chrono>
#include <thread>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define CGROUP_CPUSET_PATH_PREFIX "/sys/fs/cgroup/cpuset"
#define CPUSET_FILE "/cpuset.cpus"

std::vector<std::uint32_t> parse_cpuset_range(std::string in)
{
    std::stringstream ss1(in);
    std::string token;
    std::vector<std::uint32_t> out;
    while (std::getline(ss1, token, ':'))
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

class Tsp_Proc
{
public:
    pid_t pid;
    std::vector<uint32_t> allowed_cores;

    Tsp_Proc()
    {

        pid = getpid();
        my_path = std::filesystem::read_symlink("/proc/self/exe");
        // Open cgroups file
        cpuset_from_cgroup = get_cgroup();
        refresh_allowed_cores();
    };

    void refresh_allowed_cores()
    {
        auto sibling_pids = get_siblings();
        std::vector<u_int32_t> siblings_affinity;
        for (auto i : sibling_pids)
        {
            auto tmp = get_sibling_affinity(i);
            siblings_affinity.insert(siblings_affinity.end(), tmp.begin(), tmp.end());
        }
        std::sort(siblings_affinity.begin(), siblings_affinity.end());
        std::set_difference(cpuset_from_cgroup.begin(), cpuset_from_cgroup.end(),
                            siblings_affinity.begin(), siblings_affinity.end(),
                            std::inserter(allowed_cores, allowed_cores.begin()));
    }

    ~Tsp_Proc() {};

private:
    std::vector<uint32_t> cpuset_from_cgroup;
    std::filesystem::path my_path;

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

    std::vector<uint32_t> get_sibling_affinity(pid_t pid)
    {
        std::vector<u_int32_t> out;
        cpu_set_t mask;
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
            throw(std::runtime_error("Cgroup file for process " + std::to_string(pid) + " not found"));
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
            throw(std::runtime_error("Unable to open cgroup file " + cgroup_fn.string()));
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
            throw(std::runtime_error("Unable to open cpuset file " + cpuset_path.string()));
        }
    }
};

int main(int argc, char *argv[])
{
    // Parse args: only take the ones we use for now
    int c;
    std::uint32_t nslots(1);
    bool disappear_output = 0;

    while ((c = getopt(argc, argv, "+nfN:")) != -1)
    {
        switch (c)
        {
        case 'n':
            // Pipe stdout and stderr of forked process to /dev/null
            disappear_output = 1;
            break;
        case 'f':
            // This does nothing
            break;
        case 'N':
            nslots = std::stoul(optarg);
            break;
        }
    }

    std::vector<char *> proc_to_run;
    for (int i = optind; i < argc; i++)
    {
        proc_to_run.push_back(argv[i]);
    }

    Tsp_Proc me;

    cpu_set_t mask;
    CPU_ZERO(&mask);

    while (me.allowed_cores.size() < nslots)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        me.refresh_allowed_cores();
    }
    for (u_int32_t i = 0; i < nslots; i++)
    {
        CPU_SET(me.allowed_cores[i], &mask);
    }
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1)
    {
        throw std::runtime_error("Unable to set CPU affinity");
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
            ret = execvp(proc_to_run[0], &proc_to_run[0]);
            if (disappear_output)
            {
                close(fd);
            }

            if (ret != 0)
            {
                throw std::runtime_error("Error: could not exec after creating namespace " + std::string(proc_to_run[0]) + ". ret=" + std::to_string(fork_pid) + "errno=" + std::to_string(errno));
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
            // TODO
            // monitor child processes and make sure they
            // stay where they belong.
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