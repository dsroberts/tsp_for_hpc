#include "proc_affinity.hpp"

#include <cstdint>
#include <format>
#include <hwloc.h>
#include <string_view>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "functions.hpp"
#include "status_manager.hpp"

namespace tsp {

Proc_affinity::Proc_affinity(Status_Manager &sm, int32_t nslots, pid_t pid)
    : error_string(), sm_(sm), nslots_(nslots), pid_(pid) {

  if (hwloc_topology_init(&topology_) == -1) {
    die_with_err_errno("Failed to initialise topology object", -1);
  }
  if (hwloc_topology_set_flags(
          topology_, HWLOC_TOPOLOGY_FLAG_IS_THISSYSTEM |
                         HWLOC_TOPOLOGY_FLAG_RESTRICT_TO_CPUBINDING |
                         HWLOC_TOPOLOGY_FLAG_DONT_CHANGE_BINDING) == -1) {
    error_string = "Failed to set topology loading flags";
    return;
  }

  if (hwloc_topology_load(topology_) == -1) {
    error_string = "Failed to load topology";
    return;
  }
  auto cgroup_size = hwloc_get_nbobjs_by_type(topology_, HWLOC_OBJ_CORE);
  if (cgroup_size < 1) {
    error_string = "Failed to retrieve number of available CPU cores";
    return;
  }

  sm_.set_total_slots(cgroup_size);

  if (nslots > cgroup_size) {
    error_string = "More slots requested than available on the system, this "
                   "process can never run.";
    return;
  }
  if ((cpuset_mine_ = hwloc_bitmap_alloc()) == nullptr) {
    error_string = "Unable to allocate hwloc bitmap for this process's cpuset";
    return;
  }
}

Proc_affinity::~Proc_affinity() {
  hwloc_bitmap_free(cpuset_mine_);
  hwloc_topology_destroy(topology_);
}

void Proc_affinity::bind(std::vector<uint32_t> in) {

  for (const auto i : in) {
    if (hwloc_bitmap_set(cpuset_mine_, i) == -1) {
      error_string = "Unable to construct process binding bitmap";
      return;
    }
  }

  if (hwloc_set_cpubind(topology_, cpuset_mine_,
                        HWLOC_CPUBIND_PROCESS | HWLOC_CPUBIND_NOMEMBIND |
                            HWLOC_CPUBIND_STRICT) == -1) {
    error_string = "Unable to bind process";
    return;
  }

  return;
}

std::vector<pid_t> Proc_affinity::get_siblings() {
  return sm_.get_running_job_pids(pid_);
}
}; // namespace tsp