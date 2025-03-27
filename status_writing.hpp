#pragma once

#include <cstdint>

#include "status_manager.hpp"

namespace tsp {

enum class TimeCategory {
  queue,
  run,
  total,
};

enum class Action {
  list,
  stdout,
  stderr,
  info,
  print_time,
  github_summary,
};

int do_writer_action(Action a);
int do_writer_action(Action a, ListCategory c);
int do_writer_action(Action a, uint32_t jobid);
int do_writer_action(Action a, TimeCategory c);
int do_writer_action(Action a, TimeCategory c, uint32_t jobid);
} // namespace tsp
