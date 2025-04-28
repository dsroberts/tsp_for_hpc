#include "proc_tools.hpp"

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

std::vector<uint32_t> parse_cpuset_range(std::string in) {
  std::stringstream ss1(in);
  std::string token;
  std::vector<std::uint32_t> out;
  while (std::getline(ss1, token, ',')) {
    if (token.find('-') == std::string::npos) {
      out.push_back(std::stoul(token));
    } else {
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

std::vector<uint32_t> get_cgroup() {
  static std::string cgroup_fn("/proc/self/cgroup");
#ifdef CGROUPV2
  static std::string cgroup_cpuset_path_prefix("/sys/fs/cgroup");
  static std::string cpuset_filename("/cpuset.cpus.effective");
#else
  static std::string cgroup_cpuset_path_prefix("/sys/fs/cgroup/cpuset");
  static std::string cpuset_filename("/cpuset.cpus");
#endif
  if (!std::filesystem::exists(cgroup_fn)) {
    die_with_err("Cgroup file for current process not found", -1);
  }
  std::string line;
  std::string cpuset_path;
  // get cpuset path
  std::ifstream cgroup_file(cgroup_fn);
  if (cgroup_file.is_open()) {
    while (std::getline(cgroup_file, line)) {
      // std::vector<std::string> seglist;
      std::string segment;
      std::stringstream ss(line);
      std::getline(ss, segment, ':');
      std::getline(ss, segment, ':');
      if (segment == "cpuset") {
        std::getline(ss, segment, ':');
        cpuset_path = std::format("{}{}{}", cgroup_cpuset_path_prefix, segment,
                                  cpuset_filename);
        break;
      }
    }
  } else {
    die_with_err("Unable to open cgroup file " + cgroup_fn, -1);
  }
  // First pattern didn't work, maybe we'll have more luck if we look for
  // a blank middle segment
  cgroup_file.clear();
  cgroup_file.seekg(0, std::ios::beg);
  if (cpuset_path.empty()) {
    while (std::getline(cgroup_file, line)) {
      std::string segment;
      std::stringstream ss(line);
      std::getline(ss, segment, ':');
      std::getline(ss, segment, ':');
      if (segment.empty()) {
        std::getline(ss, segment, ':');
        cpuset_path = std::format("{}{}{}", cgroup_cpuset_path_prefix, segment,
                                  cpuset_filename);
      }
    }
  }
  cgroup_file.close();
  // read cpuset file
  std::ifstream cpuset_file(cpuset_path);
  if (cpuset_file.is_open()) {
    std::getline(cpuset_file, line);
    return parse_cpuset_range(line);
  }
  // Try the system-wide cgroup file
  cpuset_file.open(cgroup_cpuset_path_prefix + cpuset_filename);
  if (cpuset_file.is_open()) {
    std::getline(cpuset_file, line);
    return parse_cpuset_range(line);
  }
  die_with_err("Unable to open cpuset file", -1);
  return {
      0,
  };
};

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