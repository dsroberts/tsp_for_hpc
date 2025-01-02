#include "proc_affinity.hpp"

#include <algorithm>
#include <fstream>
#include <numeric>

#include "functions.hpp"

namespace tsp {

Proc_affinity::Proc_affinity(uint32_t nslots, pid_t pid)
    : nslots(nslots), pid(pid),
      my_path(std::filesystem::read_symlink("/proc/self/exe")),
      cpuset_from_cgroup(get_cgroup()) {
  // Open cgroups file
  if (nslots > cpuset_from_cgroup.size()) {
    die_with_err("More slots requested than available on the system, this "
                 "process can never run.",
                 -1);
  }
  CPU_ZERO(&mask);
}

std::vector<uint32_t> Proc_affinity::bind() {
  auto sibling_pids = get_siblings();
  std::vector<uint32_t> siblings_affinity;
  std::vector<uint32_t> out;
  for (auto i : sibling_pids) {
    auto tmp = get_sibling_affinity(i);
    siblings_affinity.insert(siblings_affinity.end(), tmp.begin(), tmp.end());
  }
  std::sort(siblings_affinity.begin(), siblings_affinity.end());
  std::vector<uint32_t> allowed_cores;
  std::set_difference(cpuset_from_cgroup.begin(), cpuset_from_cgroup.end(),
                      siblings_affinity.begin(), siblings_affinity.end(),
                      std::inserter(allowed_cores, allowed_cores.begin()));

  for (auto i = 0ul; i < nslots; i++) {
    CPU_SET(allowed_cores[i], &mask);
    out.push_back(allowed_cores[i]);
  }
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) == -1) {
    die_with_err_errno("Unable to set CPU affinity", -1);
  }
  return out;
}

std::vector<pid_t> Proc_affinity::get_siblings() {
  std::vector<pid_t> out;
  // Find all the other versions of this application running
  for (const auto &entry : std::filesystem::directory_iterator("/proc")) {
    if (std::find(skip_paths.begin(), skip_paths.end(),
                  entry.path().filename()) != skip_paths.end()) {
      continue;
    }
    if (std::filesystem::exists(entry.path() / "exe")) {
      try {
        if (std::filesystem::read_symlink(entry.path() / "exe") == my_path) {
          out.push_back(std::stoul(entry.path().filename()));
        }
      } catch (std::filesystem::filesystem_error &e) {
        // process went away
        continue;
      }
    }
  }
  return out;
};

std::vector<uint32_t> Proc_affinity::get_sibling_affinity(pid_t pid) {
  std::vector<uint32_t> out;
  cpu_set_t mask;
  // Just return an empty vector if the semaphore file is present
  if (sched_getaffinity(pid, sizeof(mask), &mask) == -1) {
    // Process may have been killed - so it isn't taking
    // resources any more
    return out;
  }
  for (const auto &i : cpuset_from_cgroup) {
    if (CPU_ISSET(i, &mask)) {
      out.push_back(i);
    }
  }
  return out;
};
}; // namespace tsp