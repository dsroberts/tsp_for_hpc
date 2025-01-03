
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
    : stdout_fn_{get_tmp() / (out_file_template + jobid)},
      stderr_fn_{get_tmp() / (err_file_template + jobid)}, disappear_{disappear},
      separate_stderr_{separate_stderr} {}
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
  //std::filesystem::remove(stdout_fn);
  out_bufs_.first = ss_out.str();

  std::ifstream stderr_stream(stderr_fn_);
  std::stringstream ss_err{};
  ss_err << stdout_stream.rdbuf();
  stderr_stream.close();
  std::filesystem::remove(stderr_fn_);
  out_bufs_.second = ss_err.str();

  return out_bufs_;
}
Output_handler::~Output_handler() {
  if (stdout_fd_ != -1) {
    close(stdout_fd_);
  }
  if (stderr_fd_ != -1) {
    close(stderr_fd_);
  }
}
} // namespace tsp