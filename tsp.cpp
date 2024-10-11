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
#include <ranges>
#include <cstring>

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
        std::vector<uint32_t> siblings_affinity;
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
        std::vector<uint32_t> out;
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

bool check_mpi(std::vector<char *> in)
{
    std::string prog_name(in[0]);
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
            execlp(prog_name.c_str(), prog_name.c_str(), "--version");
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

std::filesystem::path make_rankfile(std::vector<uint32_t> procs, int nslots)
{
    std::filesystem::path rank_file = "/tmp/" + std::to_string(getpid()) + "_rankfile.txt";
    std::ofstream rf_stream(rank_file);
    if (rf_stream.is_open())
    {
        for (uint32_t i; i < std::ranges::take_view(procs, nslots).size(); i++)
        {
            rf_stream << "rank " + std::to_string(i) + "=localhost slot=" + std::to_string(procs[i]) << std::endl;
        }
    }
    rf_stream.close();
    return rank_file;
}

void add_rankfile(std::vector<char *> *cmdline, std::filesystem::path rf)
{
    const char *rf_str = rf.c_str();
    // I know, I know. Its like 20 bytes and it runs once
    // I figure we can spare it.
    char *rf_copy = strdup(rf_str);
    char *dash_rf = strdup("-rf");
    cmdline->insert(cmdline->begin() + 1, rf_copy);
    cmdline->insert(cmdline->begin() + 1, dash_rf);
    cmdline->push_back(nullptr);
}

int main(int argc, char *argv[])
{
    // Parse args: only take the ones we use for now
    int c;
    std::uint32_t nslots(1);
    bool disappear_output = false;

    while ((c = getopt(argc, argv, "+nfN:")) != -1)
    {
        switch (c)
        {
        case 'n':
            // Pipe stdout and stderr of forked process to /dev/null
            disappear_output = true;
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

    bool is_openmpi = check_mpi(proc_to_run);

    Tsp_Proc me;

    cpu_set_t mask;
    CPU_ZERO(&mask);

    while (me.allowed_cores.size() < nslots)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        me.refresh_allowed_cores();
    }
    for (uint32_t i = 0; i < nslots; i++)
    {
        CPU_SET(me.allowed_cores[i], &mask);
    }
    if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1)
    {
        throw std::runtime_error("Unable to set CPU affinity");
    }
    std::filesystem::path rf_path;
    if (is_openmpi)
    {
        rf_path = make_rankfile(me.allowed_cores, nslots);
        add_rankfile(&proc_to_run, rf_path);
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
            if (is_openmpi)
            {
                setenv("OMPI_MCA_rmaps_base_mapping_policy", "", 1);
            }
            ret = execvp(proc_to_run[0], &proc_to_run[0]);
            if (disappear_output)
            {
                close(fd);
            }

            if (ret != 0)
            {
                throw std::runtime_error("Error: could not exec " + std::string(proc_to_run[0]) + ". ret=" + std::to_string(fork_pid) + "errno=" + std::to_string(errno));
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
    if (is_openmpi)
    {
        std::filesystem::remove(rf_path);
    }
    exit(WEXITSTATUS(fork_stat));
}