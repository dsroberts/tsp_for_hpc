#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tsp {

const std::filesystem::path get_tmp();
void die_with_err(std::string msg, int status);
void die_with_err_errno(std::string msg, int status);
int64_t now();
std::string format_hh_mm_ss(int64_t us_duration);

} // namespace tsp