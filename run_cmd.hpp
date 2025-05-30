#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tsp {
class Run_cmd {
public:
  const bool is_openmpi;
  Run_cmd(char *cmdline[], int start, int end);
  Run_cmd(std::string serialised);
  ~Run_cmd();
  std::vector<std::string> get();
  std::string print();
  const char *get_argv_0();
  char **get_argv();
  void add_rankfile(std::vector<uint32_t> procs, uint32_t nslots);

private:
  std::vector<std::string> proc_to_run_;
  std::string rf_name_;
  char **argv_holder_ = nullptr;
  void make_rankfile(std::vector<uint32_t> procs, uint32_t nslots);
  bool check_mpi(const char *exe_name);
};
} // namespace tsp