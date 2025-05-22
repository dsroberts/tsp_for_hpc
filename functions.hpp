#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tsp {

const std::filesystem::path get_tmp();
void die_with_err(std::string_view msg, int status);
void die_with_err_errno(std::string_view msg, int status);
int64_t now();
std::string format_hh_mm_ss(int64_t us_duration);

template <typename Tuple, std::size_t... Is>
auto pop_front_impl(const Tuple &tuple, std::index_sequence<Is...>) {
  return std::make_tuple(std::get<1 + Is>(tuple)...);
}

template <typename Tuple> auto pop_front(const Tuple &tuple) {
  return pop_front_impl(
      tuple, std::make_index_sequence<std::tuple_size<Tuple>::value - 1>());
}

} // namespace tsp