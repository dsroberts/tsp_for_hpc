
#include "output_manager.hpp"

#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "functions.hpp"

namespace tsp {
Output_handler::Output_handler(bool disappear, bool separate_stderr,
                               std::string jobid, bool rw)
    : stdout_fn_{get_tmp() / (std::string(out_file_template) + jobid)},
      stderr_fn_{get_tmp() / (std::string(err_file_template) + jobid)},
      stdout_fd_(-1), stderr_fd_(-1), disappear_{disappear},
      separate_stderr_{separate_stderr} {}

Output_handler::~Output_handler() {
  if (stdout_fd_ != -1) {
    close(stdout_fd_);
  }
  if (stderr_fd_ != -1) {
    close(stderr_fd_);
  }
}

void Output_handler::init_pipes() {
  if (disappear_) {
    stdout_fd_ = open("/dev/null", O_WRONLY | O_CREAT, 0666);
    stderr_fd_ = open("/dev/null", O_WRONLY | O_CREAT, 0666);
  } else {
    stdout_fd_ = open(stdout_fn_.c_str(), O_WRONLY | O_CREAT, 0600);

    if (separate_stderr_) {
      stderr_fd_ = open(stderr_fn_.c_str(), O_WRONLY | O_CREAT, 0600);
    } else {
      stderr_fd_ = stdout_fd_;
    }
  }
  dup2(stdout_fd_, 1);
  dup2(stderr_fd_, 2);
}

std::pair<std::string, std::string> Output_handler::get_output() {
  std::ifstream stdout_stream(stdout_fn_);
  std::stringstream ss_out{};
  ss_out << stdout_stream.rdbuf();
  stdout_stream.close();
  std::filesystem::remove(stdout_fn_);

  std::ifstream stderr_stream(stderr_fn_);
  std::stringstream ss_err{};
  ss_err << stdout_stream.rdbuf();
  stderr_stream.close();
  std::filesystem::remove(stderr_fn_);

  return {ss_out.str(), ss_err.str()};
}
} // namespace tsp