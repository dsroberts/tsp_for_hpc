#pragma once

#include <cstdint>
#include <hwloc.h>
#include <string_view>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "status_manager.hpp"

namespace tsp {
class Proc_affinity {
public:
  Proc_affinity(Status_Manager &sm, int32_t nslots, pid_t pid);
  ~Proc_affinity();
  std::vector<uint32_t> bind();
  std::string_view error_string;

private:
  Status_Manager &sm_;
  hwloc_topology_t topology_;
  hwloc_const_bitmap_t cpuset_all_;
  hwloc_bitmap_t cpuset_mine_;
  const int32_t nslots_;
  const pid_t pid_;
  std::vector<pid_t> get_siblings();
};
} // namespace tsp