#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.hpp"
#include "functions.hpp"
#include "jitter.hpp"
#include "locker.hpp"
#include "output_manager.hpp"
#include "proc_affinity.hpp"
#include "run_cmd.hpp"
#include "status_manager.hpp"

constexpr std::chrono::milliseconds base_wait_period{2000};

int main(int argc, char *argv[]) {

  auto config = tsp::Config(argc, argv);
  auto rerun = (config.get_int("rerun") >= 0);

  if (!rerun) {
    if (optind == argc) {
      std::cerr << std::format(tsp::help, argv[0]) << std::endl;
      die_with_err(
          "ERROR! Requested to run a command, but no command specified", -1);
    }
  }

  if (config.get_bool("do_fork")) {
    auto main_fork_pid = pid_t{fork()};
    if (main_fork_pid == -1) {
      die_with_err("Unable to fork when forking requested", main_fork_pid);
    }
    if (main_fork_pid != 0) {
      // We're done here
      return 0;
    }
  }

  auto stat = tsp::Status_Manager{};
  auto cmd = rerun
                 ? tsp::Run_cmd{stat.get_cmd_to_rerun(config.get_int("rerun"))}
                 : tsp::Run_cmd{argv, optind, argc};
  if (rerun) {
    // This variant of add_cmd will recover category and nslots from the jobid
    stat.add_cmd(cmd, config.get_int("rerun"));
  } else {
    stat.add_cmd(cmd, config.get_string("category"), config.get_int("nslots"));
  }
  auto extern_jobid = stat.get_extern_jobid();
  std::cout << extern_jobid << std::endl;

  auto jitter = tsp::Jitter{tsp::jitter_ms};
  std::this_thread::sleep_for(tsp::jitter_ms + jitter.get());

  auto binder = tsp::Proc_affinity{stat, config.get_int("nslots"), getpid()};
  std::vector<uint32_t> bound_cores;
  {
    auto locker = tsp::Locker();
    for (;;) {
      locker.lock();
      if (stat.allowed_to_run()) {
        break;
      }
      locker.unlock();
      std::this_thread::sleep_for(base_wait_period + jitter.get());
    }
    stat.job_start();
    bound_cores = binder.bind();
  }

  if (config.get_bool("verbose")) {
    std::cout << "Job id " << extern_jobid << ": " << cmd.print()
              << "has started. Job was queued for "
              << format_hh_mm_ss(stat.stime - stat.qtime)
              << "\n Job is bound to physical CPU cores: ";
    for (const auto &c : bound_cores) {
      std::cout << c << ", ";
    }
    std::cout << std::endl;
  }

  if (cmd.is_openmpi) {
    cmd.add_rankfile(bound_cores, config.get_int("nslots"));
  }

  if (rerun) {
    const auto ps = stat.get_state(config.get_int("rerun"));
    std::filesystem::current_path(ps.wd);
    environ = ps.env_ptrs;
  }
  stat.store_state({environ, std::filesystem::current_path(), {}});

  int child_stat;
  int ret;
  pid_t waited_on_pid;
  auto handler =
      tsp::Output_handler(config.get_bool("disappear_output"),
                          config.get_bool("separate_stderr"), stat.jobid, true);
  if (0 == (waited_on_pid = fork())) {
    if (cmd.is_openmpi) {
      setenv("OMPI_MCA_rmaps_base_mapping_policy", "", 1);
      setenv("OMPI_MCA_rmaps_rank_file_physical", "true", 1);
    }
    handler.init_pipes();
    ret = execvp(cmd.get_argv_0(), cmd.get_argv());
    if (ret != 0) {
      die_with_err("Error: could not exec " + std::string(cmd.get_argv_0()),
                   ret);
    }
  }
  if (waited_on_pid == -1) {
    die_with_err("Error: could not fork subprocess to exec", waited_on_pid);
  }
  for (;;) {
    pid_t ret_pid = waitpid(-1, &child_stat, 0);
    if (ret_pid < 0) {
      if (errno == ECHILD) {
        break;
      }
    }
  }
  stat.save_output(handler.get_output());
  stat.job_end(WEXITSTATUS(child_stat));

  if (config.get_bool("verbose")) {
    std::cout << "Job id " << extern_jobid << ": " << cmd.print()
              << "finished in " << format_hh_mm_ss(stat.etime - stat.stime)
              << " with status " << WEXITSTATUS(child_stat) << std::endl;
  }

  return WEXITSTATUS(child_stat);
}