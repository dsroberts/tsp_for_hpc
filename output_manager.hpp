#pragma once
#include <string>

constexpr std::string out_file_template{"tsp.o"};
constexpr std::string err_file_template{"tsp.e"};

namespace tsp {
class Output_handler {
public:
  Output_handler(bool disappear, bool separate_stderr, std::string jobid,
                 bool rw);
  void init_pipes();
  std::pair<std::string, std::string> get_output();
  ~Output_handler();

private:
  std::string stdout_fn_{};
  std::string stderr_fn_{};
  int stdout_fd_;
  int stderr_fd_;
  bool disappear_;
  bool separate_stderr_;
  std::pair<std::string, std::string> out_bufs_{};
};
} // namespace tsp