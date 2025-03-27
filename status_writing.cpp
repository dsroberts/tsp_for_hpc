#include "status_writing.hpp"

#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <sstream>

#include "functions.hpp"
#include "output_manager.hpp"
#include "status_manager.hpp"

namespace tsp {

void print_job_stdout(Status_Manager sm_ro, uint32_t id) {
  auto details = sm_ro.get_job_details_by_id(id);
  if (details.stat.status) {
    std::cout << sm_ro.get_job_stdout(id);
  } else {
    std::ifstream stream{get_tmp() /
                         (std::string(out_file_template) + details.uuid)};
    std::cout << stream.rdbuf();
  }
};
void print_job_stderr(Status_Manager sm_ro, uint32_t id) {
  auto details = sm_ro.get_job_details_by_id(id);
  if (details.stat.status) {
    std::cout << sm_ro.get_job_stderr(id);
  } else {
    std::ifstream stream{get_tmp() /
                         (std::string(err_file_template) + details.uuid)};
    std::cout << stream.rdbuf();
  }
};
void format_jobs_table(std::vector<tsp::job_stat> jobs) {
  // Not finished
  std::cout << "ID  |      State | ExitStat |   Run Time |    Command\n";
  std::cout << "=====================================================\n";
  for (const auto &info : jobs) {
    if (!info.etime) {
      std::string state{!info.stime ? "queued" : "running"};
      std::printf("%-5d %10s                           %s\n", info.id,
                  state.c_str(), info.cmd.c_str());
    } else {
      std::printf(
          "%-5d   finished %10d%14s  %s\n", info.id, info.status.value(),
          format_hh_mm_ss(info.etime.value() - info.stime.value()).c_str(),
          info.cmd.c_str());
    }
  }
}

void format_jobs_gh_md(std::vector<tsp::job_stat> jobs) {
  std::cout << "## Case timings\nCase | Time | Success?\n---- | ----: | ----\n";
  for (auto &info : jobs) {
    if (!info.etime) {
      continue;
    }
    std::string cmd;
    auto ss = std::stringstream{info.cmd};
    std::string tok;
    bool save = (info.cmd.find("python3") == std::string::npos);
    while (getline(ss, tok, ' ')) {
      if (save) {
        cmd.append(tok);
        cmd.append(" ");
      }
      if (tok.find("python3") != std::string::npos) {
        save = true;
      }
    }
    if (info.category) {
      cmd = std::format("{}: {}", info.category.value(), cmd);
    }
    info.cmd = cmd;
  }
  std::sort(jobs.begin(), jobs.end(),
            [](tsp::job_stat a, tsp::job_stat b) { return a.cmd < b.cmd; });
  for (const auto &info : jobs) {
    if (!info.etime) {
      continue;
    }
    std::cout << info.cmd << " | "
              << format_hh_mm_ss(info.etime.value() - info.stime.value())
              << " | " << (info.status == 0 ? "Yes\n" : "No\n");
  }
}

void print_job_detail(Status_Manager sm_ro, uint32_t id) {
  auto info = sm_ro.get_job_details_by_id(id);
  // Expects /etc/localtime to be symlink, therefore
  // broken on Gadi
  auto tz = std::chrono::current_zone();
  // Finished
  std::string runtime{format_hh_mm_ss(info.stat.etime.value_or(now()) -
                                      info.stat.stime.value_or(now()))};
  if (!info.stat.stime) {
    std::cout << "Status: Queued\n";
  } else if (!info.stat.etime) {
    std::cout << "Status: Running\n";
  } else {
    std::cout << "Status: Finished with exit status "
              << info.stat.status.value() << "\n";
  }
  std::cout << "Command: " << info.stat.cmd << "\n";
  std::cout << "Slots required: " << info.slots << "\n";
  std::chrono::system_clock::time_point tp{
      std::chrono::microseconds{info.stat.qtime}};
  std::cout << "Enqueue time: " << tz->to_local(tp) << "\n";
  if (info.stat.stime) {
    std::chrono::system_clock::time_point tp{
        std::chrono::microseconds{info.stat.stime.value()}};
    std::cout << "Start time: " << tz->to_local(tp) << "\n";
  }
  if (info.stat.etime) {
    std::chrono::system_clock::time_point tp{
        std::chrono::microseconds{info.stat.etime.value()}};
    std::cout << "End time: " << tz->to_local(tp) << "\n";
  }
  if (info.stat.stime) {
    std::cout << "Time run: " << runtime << "\n";
    std::cout << "TSP process pid: " << info.pid.value() << "\n";
  }
  std::cout << "Internal UUID: " << info.uuid << std::endl;
};
void print_jobs_list(Status_Manager sm_ro, ListCategory c) {
  format_jobs_table(sm_ro.get_job_stats_by_category(c));
}
void print_github_summary(Status_Manager sm_ro) {
  format_jobs_gh_md(sm_ro.get_job_stats_by_category(ListCategory::all));
};

void print_time(Status_Manager sm_ro, TimeCategory c, uint32_t jobid) {
  auto stat = sm_ro.get_job_by_id(jobid);
  switch (c) {
  case TimeCategory::queue:
    std::cout << format_hh_mm_ss(stat.stime.value_or(now()) - stat.qtime);
    break;
  case TimeCategory::run:
    if (stat.stime) {
      std::cout << format_hh_mm_ss(stat.etime.value_or(now()) -
                                   stat.stime.value());
    } else {
      std::cout << "0.000";
    }
    break;
  case TimeCategory::total:
    std::cout << format_hh_mm_ss(stat.etime.value_or(now()) - stat.qtime);
    break;
  }
  std::cout << std::endl;
}

int do_writer_action(Action a, uint32_t jobid) {
  auto sm_ro = Status_Manager(false);
  switch (a) {
  case Action::info:
    print_job_detail(sm_ro, jobid);
    break;
  case Action::stdout:
    print_job_stdout(sm_ro, jobid);
    break;
  case Action::stderr:
    print_job_stderr(sm_ro, jobid);
    break;
  default:
    die_with_err("Error! Jobid supplied for action that cannot take jobid", -1);
  }
  return EXIT_SUCCESS;
};

int do_writer_action(Action a) {
  auto sm_ro = Status_Manager(false);
  switch (a) {
  case Action::github_summary:
    print_github_summary(sm_ro);
    break;
  case Action::info:
  case Action::stdout:
  case Action::stderr:
    return do_writer_action(a, sm_ro.get_last_job_id());
    break;
  default:
    die_with_err("Error! 'list' action requested without a category", -1);
  }
  return EXIT_SUCCESS;
}

int do_writer_action(Action a, ListCategory c) {
  auto sm_ro = Status_Manager(false);
  switch (a) {
  case Action::list:
    print_jobs_list(sm_ro, c);
    break;
  default:
    die_with_err("Error! List category supplied for non-list action", -1);
  }
  return EXIT_SUCCESS;
};

int do_writer_action(Action a, TimeCategory c, uint32_t jobid) {
  auto sm_ro = Status_Manager(false);
  switch (a) {
  case Action::print_time:
    print_time(sm_ro, c, jobid);
    break;
  default:
    die_with_err("Error! Time category supplied for non-print_time action", -1);
  }
  return EXIT_SUCCESS;
};

int do_writer_action(Action a, TimeCategory c) {
  auto sm_ro = Status_Manager(false);
  return do_writer_action(a, c, sm_ro.get_last_job_id());
};
} // namespace tsp