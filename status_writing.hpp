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

void do_action(Action a);
void do_action(Action a, ListCategory c);
void do_action(Action a, uint32_t jobid);
void do_action(Action a, TimeCategory c);
void do_action(Action a, TimeCategory c, uint32_t jobid);
} // namespace tsp
