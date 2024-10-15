#include <fstream>
#include <algorithm>
#include <numeric>

#include "proc_manager.hpp"
#include "functions.hpp"
#include "semaphore.hpp"

namespace tsp
{

    Tsp_Proc::Tsp_Proc(uint32_t np) : pid(getpid()), nslots(np), my_path(std::filesystem::read_symlink("/proc/self/exe")), cpuset_from_cgroup(get_cgroup())
    {
        // Open cgroups file
        if (nslots > cpuset_from_cgroup.size())
        {
            die_with_err("More slots requested than available on the system, this process can never run.", -1);
        }
        refresh_allowed_cores();
    }

    bool Tsp_Proc::allowed_to_run()
    {
        return allowed_cores.size() > nslots;
    }

    void Tsp_Proc::refresh_allowed_cores()
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

    std::vector<pid_t> Tsp_Proc::get_siblings()
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

    std::vector<std::uint32_t> Tsp_Proc::parse_cpuset_range(std::string in)
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

    std::vector<uint32_t> Tsp_Proc::get_sibling_affinity(pid_t pid)
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

    std::vector<uint32_t> Tsp_Proc::get_cgroup()
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