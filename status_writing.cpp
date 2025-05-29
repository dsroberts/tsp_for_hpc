#include "status_writing.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>
#include <map>
#include <sstream>

#include "functions.hpp"
#include "output_manager.hpp"
#include "status_manager.hpp"

namespace tsp {

void print_job_stdout(Status_Manager sm_ro, uint32_t id) {
  auto out = sm_ro.get_job_stdout(id);
  if (out.empty()) {
    auto uuid = sm_ro.get_job_uuid(id);
    std::ifstream stream{get_tmp() / (std::string(out_file_template) + uuid)};
    std::cout << stream.rdbuf();
  } else {
    std::cout << out;
  }
};
void print_job_stderr(Status_Manager sm_ro, uint32_t id) {
  auto out = sm_ro.get_job_stderr(id);
  if (out.empty()) {
    auto uuid = sm_ro.get_job_uuid(id);
    std::ifstream stream{get_tmp() / (std::string(err_file_template) + uuid)};
    std::cout << stream.rdbuf();
  } else {
    std::cout << out;
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

void format_jobs_gh_md(std::vector<tsp::job_stat> jobs,
                       std::map<uint32_t, double> rss) {
  auto hasmem = !rss.empty();
  if (hasmem) {
    std::cout << "## Case timings\nCase | Time | MaxRSS | Success?\n---- | "
                 "----: | ----: | ----\n";
  } else {
    std::cout
        << "## Case timings\nCase | Time | Success?\n---- | ----: | ----\n";
  }
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
              << " | ";
    if (hasmem) {
      if (rss.contains(info.id)) {
        std::cout << rss[info.id] << " GB";
      }
      std::cout << " | ";
    }

    std::cout << (info.status == 0 ? "Yes\n" : "No\n");
  }
}

void print_job_detail(Status_Manager sm_ro, uint32_t id) {
  auto info = sm_ro.get_job_details_by_id(id);
  // Expects /etc/localtime to be symlink, therefore
  // broken on Gadi
  // Finished
  std::string runtime{
      format_hh_mm_ss(info.etime.value_or(now()) - info.stime.value_or(now()))};
  if (!info.stime) {
    std::cout << "Status: Queued\n";
  } else if (!info.etime) {
    std::cout << "Status: Running\n";
  } else {
    std::cout << "Status: Finished with exit status " << info.status.value()
              << "\n";
  }
  std::cout << "Command: " << info.cmd << "\n";
  std::cout << "Slots required: " << info.slots << "\n";
  std::chrono::system_clock::time_point qtp{
      std::chrono::microseconds{info.qtime}};
  std::chrono::system_clock::time_point stp;
  std::chrono::system_clock::time_point etp;
  if (info.stime) {
    stp = std::chrono::system_clock::time_point{
        std::chrono::microseconds{info.stime.value()}};
  }
  if (info.etime) {
    etp = std::chrono::system_clock::time_point{
        std::chrono::microseconds{info.etime.value()}};
  }
#ifdef __APPLE__
  // As of 2025/05 Apple clang does not support
  // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0355r7.html
  std::cout << "Enqueue time: " << qtp << "\n";
  if (info.stime) {
    std::cout << "Start time: " << stp << "\n";
  }
  if (info.etime) {
    std::cout << "End time: " << etp << "\n";
  }
#else
  auto tz = std::chrono::get_tzdb().current_zone();
  std::cout << "Enqueue time: " << tz->to_local(qtp) << "\n";
  if (info.stime) {
    std::cout << "Start time: " << tz->to_local(stp) << "\n";
  }
  if (info.etime) {
    std::cout << "End time: " << tz->to_local(etp) << "\n";
  }

#endif
  if (info.stime) {
    std::cout << "Time run: " << runtime << "\n";
    std::cout << "TSP process pid: " << info.pid.value() << "\n";
  }
  std::cout << "Internal UUID: " << info.uuid << std::endl;
};
void print_jobs_list(Status_Manager sm_ro, ListCategory c) {
  format_jobs_table(sm_ro.get_job_stats_by_category(c));
}
void print_github_summary(Status_Manager sm_ro) {
  format_jobs_gh_md(sm_ro.get_job_stats_by_category(ListCategory::all),
                    sm_ro.get_max_rss());
};

void print_time(Status_Manager sm_ro, TimeCategory c, uint32_t jobid) {
  auto stat = sm_ro.get_job_by_id(jobid);
  switch (c) {
  case TimeCategory::none:
    die_with_err("Error! Requested time information but no valid time "
                 "category provided",
                 -1);
    break;
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

int do_writer(Action a, TimeCategory time_cat, ListCategory list_cat,
              std::optional<uint32_t>(jobid)) {

  auto sm_ro = Status_Manager(false);
  switch (a) {
  case Action::none:
    die_with_err(
        "Error! Status writing requested but no valid writer action provided",
        -1);
    break;
  case Action::info:
    print_job_detail(sm_ro, jobid.value_or(sm_ro.get_last_job_id()));
    break;
  case Action::stdout:
    print_job_stdout(sm_ro, jobid.value_or(sm_ro.get_last_job_id()));
    break;
  case Action::stderr:
    print_job_stderr(sm_ro, jobid.value_or(sm_ro.get_last_job_id()));
    break;
  case Action::github_summary:
    print_github_summary(sm_ro);
    break;
  case Action::list:
    if (list_cat == ListCategory::none) {
      die_with_err(
          "Error! Requested a list but no valid list category provided", -1);
    }
    print_jobs_list(sm_ro, list_cat);
    break;
  case Action::print_time:
    if (time_cat == TimeCategory::none) {
      die_with_err("Error! Requested time information but no valid time "
                   "category provided",
                   -1);
    }
    print_time(sm_ro, time_cat, jobid.value_or(sm_ro.get_last_job_id()));
    break;
  }
  return EXIT_SUCCESS;
}
} // namespace tsp
