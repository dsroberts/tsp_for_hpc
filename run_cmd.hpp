#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace tsp {
class Run_cmd {
public:
  std::vector<char *> proc_to_run;
  const bool is_openmpi;
  Run_cmd(char *cmdline[], int start, int end);
  ~Run_cmd();
  std::string print();
  void add_rankfile(std::vector<uint32_t> procs, uint32_t nslots);

private:
  std::filesystem::path rf_name;
  void make_rankfile(std::vector<uint32_t> procs, uint32_t nslots);
  bool check_mpi(const char *exe_name);
};
} // namespace tsp