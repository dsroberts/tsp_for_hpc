#pragma once

#include <cstdint>

#include "status_manager.hpp"

namespace tsp {

enum class TimeCategory {
  none,
  queue,
  run,
  total,
};

enum class Action {
  none,
  list,
  stdout,
  stderr,
  info,
  print_time,
  github_summary,
};

int do_writer(Action a, TimeCategory time_cat, ListCategory list_cat,
              std::optional<uint32_t>(jobid));
} // namespace tsp
