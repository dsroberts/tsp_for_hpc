#include "proc_affinity.hpp"

#include <algorithm>
#include <fstream>
#include <numeric>

#include "functions.hpp"
#include "proc_tools.hpp"
#include "status_manager.hpp"

namespace tsp {

Proc_affinity::Proc_affinity(Status_Manager &sm, int32_t nslots, pid_t pid)
    : sm_(sm), nslots_(nslots), pid_(pid), cpuset_from_cgroup_(get_cgroup()) {
  // Open cgroups file
  if (static_cast<size_t>(nslots) > cpuset_from_cgroup_.size()) {
    die_with_err("More slots requested than available on the system, this "
                 "process can never run.",
                 -1);
  }
  CPU_ZERO(&mask_);
}

std::vector<uint32_t> Proc_affinity::bind() {
  auto sibling_pids = get_siblings();
  std::vector<uint32_t> siblings_affinity;
  std::vector<uint32_t> out;
  for (const auto &i : sibling_pids) {
    auto tmp = get_sibling_affinity(i);
    siblings_affinity.insert(siblings_affinity.end(), tmp.begin(), tmp.end());
  }
  std::sort(siblings_affinity.begin(), siblings_affinity.end());
  std::vector<uint32_t> allowed_cores;
  std::set_difference(cpuset_from_cgroup_.begin(), cpuset_from_cgroup_.end(),
                      siblings_affinity.begin(), siblings_affinity.end(),
                      std::inserter(allowed_cores, allowed_cores.begin()));

  for (auto i = 0l; i < nslots_; i++) {
    CPU_SET(allowed_cores[i], &mask_);
    out.push_back(allowed_cores[i]);
  }
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask_) == -1) {
    die_with_err_errno("Unable to set CPU affinity", -1);
  }
  return out;
}

std::vector<pid_t> Proc_affinity::get_siblings() {
  return sm_.get_running_job_pids(pid_);
};

std::vector<uint32_t> Proc_affinity::get_sibling_affinity(pid_t pid) {
  std::vector<uint32_t> out;
  cpu_set_t mask;
  if (sched_getaffinity(pid, sizeof(mask), &mask) == -1) {
    // Process may have been killed - so it isn't taking
    // resources any more
    return out;
  }
  for (const auto &i : cpuset_from_cgroup_) {
    if (CPU_ISSET(i, &mask)) {
      out.push_back(i);
    }
  }
  return out;
};
}; // namespace tsp