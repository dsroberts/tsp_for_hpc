#include "functions.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

const std::filesystem::path get_tmp() {
  auto tmp = std::getenv("TMPDIR");
  if (tmp != nullptr) {
    return std::filesystem::path{tmp};
  }
  tmp = std::getenv("PBS_JOBFS");
  if (tmp != nullptr) {
    return std::filesystem::path{tmp};
  } else {
    return std::filesystem::path{"/tmp"};
  }
};

void die_with_err(std::string msg, int status) {
  throw std::runtime_error(msg + "stat=" + std::to_string(status));
};

void die_with_err_errno(std::string msg, int status) {
  std::string out(msg);
  out.append("\nstat=" + std::to_string(status) +
             ", errno=" + std::to_string(errno));
  out.append(std::string("\n") + strerror(errno));
  throw std::runtime_error(out);
};

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
  static std::string cgroup_cpuset_path_prefix("/sys/fs/cgroup/cpuset");
  static std::string cpuset_filename("/cpuset.cpus");
  if (!std::filesystem::exists(cgroup_fn)) {
    throw std::runtime_error("Cgroup file for current process not found");
  }
  std::string line;
  std::filesystem::path cpuset_path;
  // get cpuset path
  std::ifstream cgroup_file(cgroup_fn);
  if (cgroup_file.is_open()) {
    while (std::getline(cgroup_file, line)) {
      std::vector<std::string> seglist;
      std::string segment;
      std::stringstream ss(line);
      while (std::getline(ss, segment, ':')) {
        seglist.push_back(segment);
      };
      if (seglist[1] == "cpuset") {
        cpuset_path = cgroup_cpuset_path_prefix;
        cpuset_path += seglist[2];
        cpuset_path += cpuset_filename;
      }
      if (!cpuset_path.empty()) {
        break;
      }
    }
    cgroup_file.close();
  } else {
    throw std::runtime_error("Unable to open cgroup file " + cgroup_fn);
  }
  // read cpuset file
  std::ifstream cpuset_file(cpuset_path);
  if (cpuset_file.is_open()) {
    std::getline(cpuset_file, line);
    return parse_cpuset_range(line);
  } else {
    throw std::runtime_error("Unable to open cpuset file " +
                             cpuset_path.string());
  }
};