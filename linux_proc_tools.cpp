#include "linux_proc_tools.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "functions.hpp"

namespace tsp {

uint64_t parse_smaps_line(std::string line) {
  auto second_field_start = line.find(":") + 1;
  auto second_field_end = line.find("k", second_field_start) - 1;
  return std::stoull(
      line.substr(second_field_start, second_field_end - second_field_start));
}

void parse_smaps(pid_t pid, mem_data &data) {
  auto smaps_file_path = std::format("/proc/{}/smaps_rollup", pid);
  std::ifstream smaps_file(smaps_file_path);
  std::string line;
  if (smaps_file.is_open()) {
    while (std::getline(smaps_file, line)) {
      if (line.starts_with("Rss:")) {
        data.rss += parse_smaps_line(line);
      }
      if (line.starts_with("Pss:")) {
        data.pss += parse_smaps_line(line);
      }
      if (line.starts_with("Shared_Clean:")) {
        data.shared += parse_smaps_line(line);
      }
      if (line.starts_with("Shared_Dirty:")) {
        data.shared += parse_smaps_line(line);
      }
      if (line.starts_with("Swap:")) {
        data.swap += parse_smaps_line(line);
      }
      if (line.starts_with("SwapPss:")) {
        data.swap_pss += parse_smaps_line(line);
      }
    }
  }
  return;
}

std::pair<pid_t,uint64_t> get_ppid_and_vmem(std::string stat_line) {
  // ppid is the 4th entry in the stat file
  std::pair<pid_t,uint64_t> out;
  std::string seg;
  std::stringstream ss(stat_line);
  for (int i = 0; i <= STAT_VSZ_FIELD; ++i) {
    std::getline(ss, seg, ' ');
    if ( i==STAT_PPID_FIELD ) {
      out.first = static_cast<pid_t>(std::stol(seg));
    }
  }
  out.second = std::stoull(seg);
  return out;
}

pid_map_t get_pid_map() {

  pid_map_t out;
  std::vector<std::string> exclude{{"self"}, {"thread-self"}};

  std::string line;
  for (auto const &dirent : std::filesystem::directory_iterator("/proc")) {
    if (dirent.is_directory()) {
      if (std::find(exclude.begin(), exclude.end(),
                    dirent.path().filename().string()) != exclude.end()) {
        continue;
      }
      auto stat_file_path = dirent.path() / "stat";
      if (std::filesystem::is_regular_file(stat_file_path)) {
        pid_t pid;
        try {
          pid = std::stol(dirent.path().filename().string());
        } catch (const std::exception &e) {
          std::cerr << e.what() << '\n'
                    << "Looks like " << dirent.path().filename()
                    << "needs to be added to the /proc path exclusions list"
                    << std::endl;
        }
        std::ifstream stat_file(stat_file_path);
        if (stat_file.is_open()) {
          std::getline(stat_file, line);
          auto ppid = get_ppid_and_vmem(line);
          out[ppid.first].push_back({pid,ppid.second});
        }
      }
    }
  }
  return out;
}

} // namespace tsp