#include "run_cmd.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "functions.hpp"

namespace tsp {
Run_cmd::Run_cmd(char *cmdline[], int start, int end)
    : is_openmpi(check_mpi(cmdline[0])), rf_name(get_tmp()) {
  for (int i = start; i < end; i++) {
    proc_to_run.push_back(cmdline[i]);
  }
  proc_to_run.push_back(nullptr);
}
Run_cmd::~Run_cmd() {
  if (is_openmpi) {
    if (!rf_name.empty()) {
      std::filesystem::remove(rf_name);
    }
  }
}

std::string Run_cmd::print() {
  std::stringstream out;
  for (const auto &i : proc_to_run) {
    out << i << " ";
  }
  return out.str();
}

void Run_cmd::add_rankfile(std::vector<uint32_t> procs, uint32_t nslots) {
  make_rankfile(procs, nslots);
  proc_to_run.insert(proc_to_run.begin() + 1, rf_name.string().data());
  proc_to_run.insert(proc_to_run.begin() + 1, std::string("rf").data());
  proc_to_run.push_back(nullptr);
}

void Run_cmd::make_rankfile(std::vector<uint32_t> procs, uint32_t nslots) {
  rf_name /= std::to_string(getpid()) + "_rankfile.txt";
  std::ofstream rf_stream(rf_name);
  if (rf_stream.is_open()) {
    for (uint32_t i = 0; i < nslots; i++) {
      rf_stream << "rank " + std::to_string(i) +
                       "=localhost slot=" + std::to_string(procs[i])
                << std::endl;
    }
  }
  rf_stream.close();
}
bool Run_cmd::check_mpi(const char *exe_name) {
  std::string prog_name(exe_name);
  if (prog_name == "mpirun" || prog_name == "mpiexec") {
    // OpenMPI does not respect parent process binding,
    // so we need to check if we're attempting to run
    // OpenMPI, and if we are, we need to construct a
    // rankfile and add it to the arguments. Note that
    // this will explode if you're attempting anything
    // other than by-core binding and mapping
    int pipefd[2];
    pipe(pipefd);
    int fork_pid;
    std::string mpi_version_output;
    if (0 == (fork_pid = fork())) {
      close(pipefd[0]);
      dup2(pipefd[1], 1);
      close(pipefd[1]);
      execlp(prog_name.c_str(), prog_name.c_str(), "--version", nullptr);
    } else {
      char buffer[1024];
      close(pipefd[1]);
      while (read(pipefd[0], buffer, sizeof(buffer)) != 0) {
        mpi_version_output.append(buffer);
      }
      close(pipefd[0]);
      if (waitpid(fork_pid, nullptr, 0) == -1) {
        throw std::runtime_error("Error watiting for mpirun test process");
      }
    }
    if (mpi_version_output.find("Open MPI") != std::string::npos ||
        mpi_version_output.find("OpenRTE") != std::string::npos) {
      // OpenMPI detected...
      return true;
    }
  }
  return false;
}
} // namespace tsp