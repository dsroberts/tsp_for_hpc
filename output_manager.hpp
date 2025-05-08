#pragma once
#include <string>
#include <string_view>

constexpr std::string_view out_file_template{"tsp.o"};
constexpr std::string_view err_file_template{"tsp.e"};

namespace tsp {
class Output_handler {
public:
  Output_handler(bool disappear, bool separate_stderr, std::string jobid,
                 bool rw);
  void init_pipes();
  std::pair<std::string, std::string> get_output();
  ~Output_handler();

private:
  const std::string stdout_fn_{};
  const std::string stderr_fn_{};
  int stdout_fd_;
  int stderr_fd_;
  const bool disappear_;
  const bool separate_stderr_;
};
} // namespace tsp