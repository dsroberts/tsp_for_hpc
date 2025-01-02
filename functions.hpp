#pragma once

#include <cstdint>
#include <filesystem>
#include <numeric>
#include <string>
#include <vector>

const std::filesystem::path get_tmp();
void die_with_err(std::string msg, int status);
void die_with_err_errno(std::string msg, int status);
std::vector<uint32_t> parse_cpuset_range(std::string in);
std::vector<uint32_t> get_cgroup();