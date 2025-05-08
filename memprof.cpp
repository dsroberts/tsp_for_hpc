#include "memprof.hpp"

#include <chrono>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#include "functions.hpp"
#include "memprof_manager.hpp"
#include "linux_proc_tools.hpp"

namespace tsp {

Memprof_config::Memprof_config() {
  bool_vars = {{"verbose", false}, {"do_fork", true}},
  int_vars = {{"polling_interval", 10}, {"idle_timeout", 30}};
}

int do_memprof(Memprof_config conf) {

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
  auto stat = tsp::Memprof_Manager();

  auto polling_interval =
      std::chrono::seconds(conf.get_int("polling_interval"));
  auto idle_timeout =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::seconds(conf.get_int("idle_timeout")) + polling_interval)
          .count();

  for (;;) {
    auto running_procs = stat.get_running_job_ids_and_pids();
    auto current_time = now();
    if (conf.get_bool("verbose")) {
      std::cout << "Checking memory usage for " << running_procs.size()
                << " jobs." << std::endl;
    }
    if (running_procs.size() == 0) {
      if (current_time - last_idle > idle_timeout) {
        if (conf.get_bool("verbose")) {
          std::cout << "Idle timeout: " << conf.get_int("idle_timeout")
                    << " seconds reached. Exiting" << std::endl;
        }
        exit(EXIT_SUCCESS);
      }
      std::vector<mem_data> to_store;

    } else {
      last_idle = current_time;
      auto pid_map = get_pid_map();
      std::vector<mem_data> to_store;

      for (const auto &[jobid, pid] : running_procs) {
        if (conf.get_bool("verbose")) {
          std::cout << "Checking job " << jobid << "\nPid: " << pid
                    << std::endl;
        }
        // Gather all subprocesses of <jobids> tsp instance
        to_store.emplace_back(jobid);
        std::vector<pid_t> pids{pid};
        for (auto i_pid = 0ul; i_pid < pids.size(); ++i_pid) {
          if (pid_map.contains(pids[i_pid])) {
            for (const auto &[j_pid, vmem] : pid_map[pids[i_pid]]) {
              pids.push_back(j_pid);
              to_store.back().vmem += vmem;
            }
          }
          parse_smaps(pids[i_pid], to_store.back());
        }
      }
      stat.memprof_update(current_time, to_store);
    }
    std::this_thread::sleep_for(polling_interval);
  }
}

} // namespace tsp