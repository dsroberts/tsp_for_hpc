#pragma once

#include <cstdint>

namespace tsp {

enum class Action {
  list,
  stdout,
  stderr,
  info,
  github_summary,
};

void do_action(Action a);
void do_action(Action a, uint32_t jobid);
} // namespace tsp
