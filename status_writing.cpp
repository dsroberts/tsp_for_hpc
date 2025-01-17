#include "status_writing.hpp"

#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>

#include "status_manager.hpp"

namespace tsp {

std::string format_hh_mm_ss(int64_t us_duration) {
  auto hh = us_duration / 3600000000ll;
  auto mm = (us_duration / 60000000ll) % 60ll;
  auto ss = (us_duration / 1000000ll) % 60ll;
  auto us = (us_duration % 1000000ll) / 10000ll;

  if (us_duration > 3599999999ll) {
    return std::format("{:2}:{:02}:{:02}.{:02}", hh, mm, ss, us);
  } else if (us_duration > 59999999ll) {
    return std::format("   {:2}:{:02}.{:02}", mm, ss, us);
  } else if (us_duration > 999999ll) {
    return std::format("      {:2}.{:02}", ss, us);
  } else {
    return std::format("        0.{:02}", us);
  }
}

void print_job_stdout(Status_Manager sm_ro, uint32_t id) {};
void print_job_stderr(Status_Manager sm_ro, uint32_t id) {};
void print_job_detail(Status_Manager sm_ro, uint32_t id) {
  auto info = sm_ro.get_job_by_id(id);
  // Not finished
  if (!info.etime) {
    std::string state{!info.stime ? "queued" : "running"};
    std::printf("%-5d %10s                    %s\n", info.id, state.c_str(),
                info.cmd.c_str());
  } else {
    std::printf(
        "%-5d   finished %6d %s %s\n", info.id, info.status.value(),
        format_hh_mm_ss(info.etime.value() - info.stime.value()).c_str(),
        info.cmd.c_str());
  }
};
void print_all_jobs() {};
void print_github_summary(Status_Manager sm_ro) {
  std::cout << sm_ro.get_last_job_id() << std::endl;
};

void do_action(Action a, uint32_t jobid) {
  auto sm_ro = Status_Manager(false);
  if (a == Action::github_summary) {
    print_github_summary(sm_ro);
  } else if (a == Action::info) {
    print_job_detail(sm_ro, jobid);
  }
  std::exit(EXIT_SUCCESS);
};

void do_action(Action a) {
  auto sm_ro = Status_Manager(false);
  if (a == Action::github_summary) {
    print_github_summary(sm_ro);
  } else if (a == Action::info) {
    auto jobid = sm_ro.get_last_job_id();
    print_job_detail(sm_ro, jobid);
  }
  std::exit(EXIT_SUCCESS);
};

} // namespace tsp