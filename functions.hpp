#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <numeric>

const char *get_tmp();
void die_with_err(std::string msg, int status);
std::vector<uint32_t> parse_cpuset_range(std::string in);
std::vector<std::uint32_t> get_cgroup();