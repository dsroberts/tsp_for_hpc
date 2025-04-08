#include "functions.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <iostream>
#include <string>
#include <vector>

namespace tsp {

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
  std::cerr << msg << std::endl;
  std::cerr << "stat=" << status << std::endl;
  std::exit(EXIT_FAILURE);
};

void die_with_err_errno(std::string msg, int status) {
  std::cerr << msg << std::endl;
  std::cerr << "stat=" << status << ", errno=" << errno << std::endl;
  std::cerr << strerror(errno) << std::endl;
  std::exit(EXIT_FAILURE);
};

int64_t now() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string format_hh_mm_ss(int64_t us_duration) {
  auto hh = us_duration / 3600000000ll;
  auto mm = (us_duration / 60000000ll) % 60ll;
  auto ss = (us_duration / 1000000ll) % 60ll;
  auto us = (us_duration % 1000000ll) / 1000ll;

  if (us_duration > 3599999999ll) {
    return std::format("{}:{:02}:{:02}.{:03}", hh, mm, ss, us);
  } else if (us_duration > 59999999ll) {
    return std::format("{}:{:02}.{:03}", mm, ss, us);
  } else if (us_duration > 999999ll) {
    return std::format("{}.{:03}", ss, us);
  } else {
    return std::format("0.{:03}", us);
  }
}

} // namespace tsp