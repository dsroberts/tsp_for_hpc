#include "timeout.hpp"

#include <chrono>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include "functions.hpp"
#include "status_manager.hpp"

namespace tsp {

Timeout_config::Timeout_config() {
  bool_vars = {{"verbose", false}, {"do_fork", true}};
  int_vars = {
      {"polling_interval", 10}, {"idle_timeout", 30}, {"job_timeout", 7200}};
}

int do_timeout(Timeout_config conf) {

  if (conf.get_bool("do_fork")) {
    auto main_fork_pid = pid_t{fork()};
    if (main_fork_pid == -1) {
      die_with_err("Unable to fork when forking requested", main_fork_pid);
    }
    if (main_fork_pid != 0) {
      // We're done here
      return 0;
    }
  }

  auto last_idle = now();
  auto stat_ro = tsp::Status_Manager{false, false};
  auto polling_interval =
      std::chrono::seconds(conf.get_int("polling_interval"));
  auto idle_timeout =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::seconds(conf.get_int("idle_timeout")) + polling_interval)
          .count();
  auto job_timeout = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::seconds(conf.get_int("job_timeout")))
                         .count();

  for (;;) {

    auto running_procs =
        stat_ro.get_job_stats_by_category(ListCategory::running);

    auto current_time = now();
    if (running_procs.size() == 0) {
      if (current_time - last_idle > idle_timeout) {
        if (conf.get_bool("verbose")) {
          std::cout << "Idle timeout: " << conf.get_int("idle_timeout")
                    << " seconds reached. Exiting" << std::endl;
        }
        exit(EXIT_SUCCESS);
      }
    } else {
      last_idle = current_time;
    }
    if (conf.get_bool("verbose")) {
      std::cout << "Checking runtimes for " << running_procs.size() << " jobs."
                << std::endl;
    }
    for (const auto &job : running_procs) {
      if (conf.get_bool("verbose")) {
        std::cout << "Checking job " << job.id << "\nCommand: " << job.cmd
                  << std::endl;
      }
      if (!job.stime || !!job.etime) {
        if (conf.get_bool("verbose")) {
          std::cout << "Cannot check timing for job " << job.id
                    << " job has either not started or has ended" << std::endl;
        }
        // Somehow we've recovered a job that hasn't started
        // or has finished
        continue;
      }
      if (current_time - job.stime.value() >= job_timeout) {
        if (conf.get_bool("verbose")) {
          std::cout << "Job id: " << job.id << "\nCommand: " << job.cmd
                    << "\nHas exceeded runtime limit of "
                    << format_hh_mm_ss(job_timeout) << ". Killing" << std::endl;
        }
        auto details = stat_ro.get_job_details_by_id(job.id);

        auto kill_stat = kill(details.pid.value(), SIGTERM);
        if (kill_stat == -1) {
          if (errno != ESRCH) {
            std::cerr << "Error! Unable to kill jobid " << job.id
                      << "\nCommand: " << job.cmd
                      << "\nTSP pid: " << details.pid.value() << std::endl;
          }
        }
      }
    }
    std::this_thread::sleep_for(polling_interval);
  }
}

} // namespace tsp