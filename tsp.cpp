#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "functions.hpp"
#include "jitter.hpp"
#include "locker.hpp"
#include "output_manager.hpp"
#include "proc_affinity.hpp"
#include "run_cmd.hpp"
#include "status_manager.hpp"

constexpr std::chrono::milliseconds base_wait_period{2000};

int main(int argc, char *argv[]) {
  auto stat = tsp::Status_Manager{};
  auto category = std::string{};
  uint32_t nslots{1};
  bool disappear_output{false};
  bool do_fork{true};
  bool separate_stderr{false};

  // Parse args: only take the ones we use for now
  int c;
  while ((c = getopt(argc, argv, "+nfL:N:E")) != -1) {
    switch (c) {
    case 'n':
      // Pipe stdout and stderr of forked process to /dev/null
      disappear_output = true;
      break;
    case 'f':
      do_fork = false;
      break;
    case 'N':
      nslots = std::stoul(optarg);
      break;
    case 'E':
      separate_stderr = true;
      break;
    case 'L':
      category = std::string{optarg};
      break;
    }
  }

  auto jitter = tsp::Jitter{tsp::jitter_ms};
  std::this_thread::sleep_for(tsp::jitter_ms + jitter.get());

  auto cmd = tsp::Run_cmd{argv, optind, argc};
  stat.add_cmd(cmd, category, nslots);

  if (do_fork) {
    pid_t main_fork_pid;
    main_fork_pid = fork();
    if (main_fork_pid == -1) {
      die_with_err("Unable to fork when forking requested", main_fork_pid);
    }
    if (main_fork_pid != 0) {
      // We're done here
      return 0;
    }
  }

  auto binder = tsp::Proc_affinity{nslots, getpid()};
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

  if (cmd.is_openmpi) {
    cmd.add_rankfile(bound_cores, nslots);
  }

  pid_t fork_pid;
  int fork_stat;
  // exec & monitor here.
  if (0 == (fork_pid = fork())) {

    // We are now init, so fork again, and wait in a loop until it returns
    // ECHILD
    int child_stat;
    int ret;
    pid_t waited_on_pid;
    auto handler = tsp::Output_handler(disappear_output, separate_stderr,
                                       stat.jobid, true);
    if (0 == (waited_on_pid = fork())) {
      if (cmd.is_openmpi) {
        setenv("OMPI_MCA_rmaps_base_mapping_policy", "", 1);
        setenv("OMPI_MCA_rmaps_rank_file_physical", "true", 1);
      }
      handler.init_pipes();
      ret = execvp(cmd.proc_to_run[0], cmd.proc_to_run.data());
      if (ret != 0) {
        die_with_err("Error: could not exec " + std::string(cmd.proc_to_run[0]),
                     ret);
      }
    }
    if (waited_on_pid == -1) {
      die_with_err("Error: could not fork init subprocess", waited_on_pid);
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
    return WEXITSTATUS(child_stat);
  }
  if (fork_pid == -1) {
    die_with_err("Error: could not fork into new pid namespace", fork_pid);
  }

  pid_t ret_pid = waitpid(fork_pid, &fork_stat, 0);
  if (ret_pid == -1) {
    die_with_err("Error: failed to wait for forked process", ret_pid);
  }

  // Exit with status of forked process.
  stat.job_end(WEXITSTATUS(fork_stat));
  return WEXITSTATUS(fork_stat);
}