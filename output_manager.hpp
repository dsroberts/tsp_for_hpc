#pragma once
#include <string>

constexpr std::string out_file_template{"tsp.o"};
constexpr std::string err_file_template{"tsp.e"};

namespace tsp {
class Output_handler {
public:
  int stdout_fd;
  int stderr_fd;
  Output_handler(bool disappear, bool separate_stderr, std::string jobid,
                 bool rw);
  void init_pipes();
  std::pair<std::string, std::string> get_output();
  ~Output_handler();

private:
  std::string stdout_fn{};
  std::string stderr_fn{};
  bool disappear_;
  bool separate_stderr_;
  std::pair<std::string, std::string> out_bufs{};
};
} // namespace tsp