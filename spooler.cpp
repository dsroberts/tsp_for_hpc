#include "spooler.hpp"

#include <cstdint>
#include <format>
#include <iostream>
#include <map>
#include <string>
#include <thread>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "functions.hpp"
#include "help.hpp"
#include "jitter.hpp"
#include "locker.hpp"
#include "output_manager.hpp"
#include "proc_affinity.hpp"
#include "status_manager.hpp"

bool time_to_die = false;
int seen_signal = 0;

void sigintHandlerPostFork(int sig) {
  // disable this signal for us
  signal(sig, SIG_IGN);
  // pass it on - we can clean up when
  // all child processes have exited
  kill(0, sig);
}

void sigintHandlerPreFork(int sig) {
  // Ensure the database is updated to reflect
  // we're no longer in the queue if we're killed
  // before launching our process
  time_to_die = true;
  seen_signal = sig;
}

extern char **environ;

auto signals_to_forward = {SIGINT, SIGHUP, SIGTERM};

namespace tsp {

constexpr std::chrono::milliseconds base_wait_period{2000};

Spooler_config::Spooler_config() {
  bool_vars = {{"disappear_output", false},
               {"do_fork", true},
               {"separate_stderr", false},
               {"verbose", false},
#ifdef __APPLE__
               {"binding", false}};
#else
               {"binding", true}};
#endif
  int_vars = {{"nslots", 1}, {"rerun", -1}};
}

int do_spooler(Spooler_config config, int argc, int optind, char *argv[]) {

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
  for (const auto sig : signals_to_forward) {
    signal(sig, sigintHandlerPreFork);
  }
  auto extern_jobid = stat.get_extern_jobid();
  std::cout << extern_jobid << std::endl;

  auto jitter = tsp::Jitter{tsp::jitter_ms};
  std::this_thread::sleep_for(tsp::jitter_ms + jitter.get());

  auto binder = tsp::Proc_affinity{stat, config.get_int("nslots"), getpid()};
  if (!binder.error_string.empty()) {
    stat.job_end(-1);
    die_with_err(binder.error_string, -1);
  }
  std::vector<uint32_t> bound_cores;
  {
    auto locker = tsp::Locker();
    for (;;) {
      if (time_to_die) {
        stat.job_end(128 + seen_signal);
        std::exit(EXIT_FAILURE);
      }
      locker.lock();
      if (stat.allowed_to_run()) {
        break;
      }
      locker.unlock();
      std::this_thread::sleep_for(base_wait_period + jitter.get());
    }
    stat.job_start();
    if (config.get_bool("binding")) {
      bound_cores = binder.bind();
      if (!binder.error_string.empty()) {
        stat.job_end(-1);
        die_with_err_errno(binder.error_string, -1);
      }
    }
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

  if (cmd.is_openmpi && config.get_bool("binding")) {
    cmd.add_rankfile(bound_cores, config.get_int("nslots"));
  }

  if (rerun) {
    const auto ps = stat.get_state(config.get_int("rerun"));
    std::filesystem::current_path(ps.wd);
    environ = ps.env.first;
  }
  stat.store_state({{environ, {}}, std::filesystem::current_path()});

  int child_stat;
  int ret;
  pid_t waited_on_pid;
  auto handler =
      tsp::Output_handler(config.get_bool("disappear_output"),
                          config.get_bool("separate_stderr"), stat.jobid, true);
  // Might have been signalled between start and here
  if (time_to_die) {
    stat.job_end(128 + seen_signal);
    std::exit(EXIT_FAILURE);
  }
  // Create our own process group here for signal handling purposes
  if (setpgid(0, 0) == -1) {
    stat.job_end(-1);
    die_with_err_errno("Unable to set process group id", -1);
  }
  if (0 == (waited_on_pid = fork())) {
    if (cmd.is_openmpi) {
      setenv("OMPI_MCA_rmaps_base_mapping_policy", "", 1);
      setenv("OMPI_MCA_rmaps_rank_file_physical", "true", 1);
    }
    handler.init_pipes();
    ret = execvp(cmd.get_argv_0(), cmd.get_argv());
    if (ret != 0) {
      die_with_err(std::format("Error: could not exec {}",
                               std::string(cmd.get_argv_0())),
                   ret);
    }
  }
  if (waited_on_pid == -1) {
    stat.job_end(-1);
    die_with_err("Error: could not fork subprocess to exec", waited_on_pid);
  }
  for (const auto sig : signals_to_forward) {
    signal(sig, sigintHandlerPostFork);
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
  int child_exit_stat = -1;
  if (WIFEXITED(child_stat)) {
    child_exit_stat = WEXITSTATUS(child_stat);
  } else if (WIFSIGNALED(child_stat)) {
    // PBSPro convention - status = 128 + signal
    child_exit_stat = 128 + WTERMSIG(child_stat);
  }

  stat.job_end(child_exit_stat);

  if (config.get_bool("verbose")) {
    std::cout << "Job id " << extern_jobid << ": " << cmd.print()
              << "finished in " << format_hh_mm_ss(stat.etime - stat.stime)
              << " with status " << WEXITSTATUS(child_stat) << std::endl;
  }

  return WEXITSTATUS(child_stat);
}
} // namespace tsp