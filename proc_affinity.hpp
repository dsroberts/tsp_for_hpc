#pragma once

#include <vector>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <unistd.h>
#include <filesystem>
#include <sys/types.h>

namespace tsp
{
    class Proc_affinity
    {
    public:
        Proc_affinity(uint32_t nslots, pid_t pid);
        ~Proc_affinity() {};
        std::vector<uint32_t> bind();

    private:
        const uint32_t nslots;
        const pid_t pid;
        const std::filesystem::path my_path;
        const std::vector<uint32_t> cpuset_from_cgroup;
        cpu_set_t mask;
        std::vector<pid_t> get_siblings();
        std::vector<uint32_t> get_sibling_affinity(pid_t pid);
        std::vector<std::string> skip_paths = {std::to_string(pid), "self", "thread-self"};
    };
}