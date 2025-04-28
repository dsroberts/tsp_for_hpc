#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace tsp {

struct mem_data {
  uint32_t jobid;
  uint64_t vmem;
  uint64_t rss;
  uint64_t pss;
  uint64_t shared;
  uint64_t swap;
  uint64_t swap_pss;

  mem_data(uint32_t jobid)
      : jobid(jobid), vmem(0ull), rss(0ull), pss(0ull), shared(0ull),
        swap(0ull), swap_pss(0ull) {}
};

typedef std::map<pid_t, std::vector<std::pair<pid_t,uint64_t>>> pid_map_t;

constexpr int STAT_PPID_FIELD = 3;
constexpr int STAT_VSZ_FIELD = 22;

std::vector<uint32_t> parse_cpuset_range(std::string in);
std::vector<uint32_t> get_cgroup();
void parse_smaps(pid_t pid, mem_data &data);
std::pair<pid_t,uint64_t> get_ppid_and_vmem(std::string);
pid_map_t get_pid_map();

} // namespace tsp
