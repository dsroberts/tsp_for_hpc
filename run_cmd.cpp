#include "run_cmd.hpp"

#include <cstdint>
#include <cstring>
#include <format>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "functions.hpp"

namespace tsp {
Run_cmd::Run_cmd(char *cmdline[], int start, int end)
    : is_openmpi(check_mpi(cmdline[start])) {
  for (int i = start; i < end; i++) {
    proc_to_run_.emplace_back(cmdline[i]);
  }
}

Run_cmd::Run_cmd(std::string serialised)
    : is_openmpi(check_mpi(serialised.c_str())) {
  auto start = 0ul;
  auto end = serialised.find('\0');
  while (end != std::string::npos) {
    proc_to_run_.emplace_back(serialised.substr(start, end - start));
    start = end + 1;
    end = serialised.find('\0', start);
  }
  // Final element will be an empty string, so remove it
  proc_to_run_.pop_back();
}

Run_cmd::~Run_cmd() {
  if (is_openmpi) {
    if (!rf_name_.empty()) {
      std::filesystem::remove(rf_name_);
    }
    if (argv_holder_ != nullptr) {
      free(argv_holder_);
    }
  }
}

std::vector<std::string> Run_cmd::get() { return proc_to_run_; }

std::string Run_cmd::print() {
  std::stringstream out;
  for (const auto &i : proc_to_run_) {
    if (!i.empty()) {
      out << i << " ";
    }
  }
  return out.str();
}

const char *Run_cmd::get_argv_0() { return proc_to_run_[0].c_str(); }

char **Run_cmd::get_argv() {
  // Do it the old fashioned way
  if (argv_holder_ == nullptr) {
    if (nullptr == (argv_holder_ = static_cast<char **>(
                        malloc((proc_to_run_.size() + 1) * sizeof(char *))))) {
      die_with_err_errno("Malloc failed", -1);
    }
    for (auto i = 0ul; i < proc_to_run_.size(); ++i) {
      auto &p = proc_to_run_[i];
      argv_holder_[i] = p.data();
    }
    argv_holder_[proc_to_run_.size()] = nullptr;
  }
  return argv_holder_;
}

void Run_cmd::add_rankfile(std::vector<uint32_t> procs, uint32_t nslots) {
  make_rankfile(procs, nslots);
  proc_to_run_.emplace(proc_to_run_.begin() + 1, rf_name_);
  proc_to_run_.emplace(proc_to_run_.begin() + 1, "--rankfile");
}

void Run_cmd::make_rankfile(std::vector<uint32_t> procs, uint32_t nslots) {
  rf_name_ = std::format(".{}_rankfile.txt",getpid());
  std::ofstream rf_stream(rf_name_);
  if (rf_stream.is_open()) {
    for (uint32_t i = 0; i < nslots; i++) {
      rf_stream << "rank " << i << "=localhost slot=" << procs[i] << std::endl;
    }
  }
  rf_stream.close();
}
bool Run_cmd::check_mpi(const char *exe_name) {
  std::string prog_name(exe_name);
  std::string prog_test(prog_name);
  if (prog_name.starts_with('/')) {
    prog_test = std::filesystem::path(prog_name).filename().string();
  }
  if (prog_test == "mpirun" || prog_test == "mpiexec") {
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
        throw std::runtime_error("Error waiting for mpirun test process");
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
