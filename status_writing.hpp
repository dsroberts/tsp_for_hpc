#pragma once

#include <cstdint>

namespace tsp {

enum class Action {
  list,
  stdout,
  stderr,
  info,
  github_summary,
  list_failed,
  list_queued,
  list_running,
};

void do_action(Action a);
void do_action(Action a, uint32_t jobid);
} // namespace tsp
