#pragma once

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "status_manager.hpp"

namespace tsp {
class Proc_affinity {
public:
  Proc_affinity(Status_Manager &sm, int32_t nslots, pid_t pid);
  std::vector<uint32_t> bind();

private:
  Status_Manager &sm_;
  const int32_t nslots_;
  const pid_t pid_;
  const std::vector<uint32_t> cpuset_from_cgroup_;
  cpu_set_t mask_;
  std::vector<pid_t> get_siblings();
  std::vector<uint32_t> get_sibling_affinity(pid_t pid);
};
} // namespace tsp