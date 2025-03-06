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
  std::string runtime;
  if (!info.stat.stime) {
    std::cout << "Status: Queued\n";
  } else if (!info.stat.etime) {
    runtime = format_hh_mm_ss(now() - info.stat.stime.value());
    std::cout << "Status: Running\n";
  } else {
    runtime =
        format_hh_mm_ss(info.stat.etime.value() - info.stat.stime.value());
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
void print_all_jobs(Status_Manager sm_ro) {
  format_jobs_table(sm_ro.get_job_stats_by_category('a'));
};
void print_failed_jobs(Status_Manager sm_ro) {
  format_jobs_table(sm_ro.get_job_stats_by_category('f'));
}
void print_queued_jobs(Status_Manager sm_ro) {
  format_jobs_table(sm_ro.get_job_stats_by_category('q'));
}
void print_running_jobs(Status_Manager sm_ro) {
  format_jobs_table(sm_ro.get_job_stats_by_category('r'));
}
void print_github_summary(Status_Manager sm_ro) {
  format_jobs_gh_md(sm_ro.get_job_stats_by_category('a'));
};

void do_action(Action a, uint32_t jobid) {
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
  std::exit(EXIT_SUCCESS);
};

void do_action(Action a) {
  auto sm_ro = Status_Manager(false);
  switch (a) {
  case Action::github_summary:
    print_github_summary(sm_ro);
    break;
  case Action::info:
    print_job_detail(sm_ro, sm_ro.get_last_job_id());
    break;
  case Action::stdout:
    print_job_stdout(sm_ro, sm_ro.get_last_job_id());
    break;
  case Action::stderr:
    print_job_stderr(sm_ro, sm_ro.get_last_job_id());
    break;
  case Action::list:
    print_all_jobs(sm_ro);
    break;
  case Action::list_failed:
    print_failed_jobs(sm_ro);
    break;
  case Action::list_queued:
    print_queued_jobs(sm_ro);
    break;
  case Action::list_running:
    print_running_jobs(sm_ro);
    break;
  }
  std::exit(EXIT_SUCCESS);
};
} // namespace tsp