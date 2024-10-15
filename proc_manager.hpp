#pragma once

#include <vector>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <unistd.h>
#include <filesystem>
#include <sys/types.h>

#define CGROUP_CPUSET_PATH_PREFIX "/sys/fs/cgroup/cpuset"
#define CPUSET_FILE "/cpuset.cpus"

namespace tsp
{
    class Tsp_Proc
    {
    public:
        const pid_t pid;
        std::vector<uint32_t> allowed_cores;
        Tsp_Proc(uint32_t nslots);
        bool allowed_to_run();
        void refresh_allowed_cores();
        ~Tsp_Proc() {};

    private:
        const uint32_t nslots;
        const std::filesystem::path my_path;
        const std::vector<uint32_t> cpuset_from_cgroup;
        std::vector<pid_t> get_siblings();
        std::vector<std::uint32_t> parse_cpuset_range(std::string in);
        std::vector<uint32_t> get_sibling_affinity(pid_t pid);
        std::vector<uint32_t> get_cgroup();
    };
}