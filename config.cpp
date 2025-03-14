#include "config.hpp"

#include <cstdint>
#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <string>

#include <getopt.h>
#include <unistd.h>

#include "functions.hpp"
#include "status_manager.hpp"
#include "status_writing.hpp"

static struct option long_options[] = {
    {"no-output", no_argument, nullptr, 'n'},
    {"no-fork", no_argument, nullptr, 'f'},
    {"nslots", required_argument, nullptr, 'N'},
    {"separate-stderr", no_argument, nullptr, 'E'},
    {"label", required_argument, nullptr, 'L'},
    {"list", no_argument, nullptr, 'l'},
    {"info", required_argument, nullptr, 'i'},
    {"db-path", no_argument, nullptr, 0},
    {"gh-summary", no_argument, nullptr, 0},
    {"list-failed", no_argument, nullptr, 0},
    {"list-queued", no_argument, nullptr, 0},
    {"list-running", no_argument, nullptr, 0},
    {"print-queue-time", required_argument, nullptr, 1},
    {"print-run-time", required_argument, nullptr, 2},
    {"print-total-time", required_argument, nullptr, 3},
    {"stdout", required_argument, nullptr, 'o'},
    {"stderr", required_argument, nullptr, 'e'},
    {"rerun", required_argument, nullptr, 'r'},
    {"verbose", no_argument, nullptr, 'v'},
    {"help", no_argument, nullptr, 'h'},
    {nullptr, 0, nullptr, 0}};

namespace tsp {
Config::Config(int argc, char *argv[])
    : bool_vars{{"disappear_output", false},
                {"do_fork", true},
                {"separate_stderr", false},
                {"verbose", false}},
      int_vars{
          {"nslots", 1},
          {"rerun", -1},
      },
      str_vars{} {

  if (argc == 1) {
    do_action(Action::list, ListCategory::all);
  }

  opterr = 0;
  int c;
  int option_index;
  while ((c = getopt_long(argc, argv, "+nfL:N:Ei:lho:e:r:v", long_options,
                          &option_index)) != -1) {
    switch (c) {
    case 'n':
      // Pipe stdout and stderr of forked process to /dev/null
      bool_vars["disappear_output"] = true;
      break;
    case 'f':
      bool_vars["do_fork"] = false;
      break;
    case 'N':
      int_vars["nslots"] = std::stoul(optarg);
      break;
    case 'E':
      bool_vars["separate_stderr"] = true;
      break;
    case 'L':
      str_vars["category"] = std::string{optarg};
      break;
    case 'v':
      bool_vars["verbose"] = true;
      break;
    case 'r':
      int_vars["rerun"] = std::stoul(optarg);
      break;
    case 'i':
      do_action(Action::info, std::stoul(optarg));
      break;
    case 'o':
      do_action(Action::stdout, std::stoul(optarg));
      break;
    case 'e':
      do_action(Action::stderr, std::stoul(optarg));
      break;
    case 'l':
      do_action(Action::list, ListCategory::all);
      break;
    case 'h':
      std::cout << std::format(help, argv[0]) << std::endl;
      exit(EXIT_SUCCESS);
      break;
    case '?':
      switch (optopt) {
      case 'i':
        do_action(Action::info);
        break;
      case 'o':
        do_action(Action::stdout);
        break;
      case 'e':
        do_action(Action::stderr);
        break;
      case 1:
        do_action(Action::print_time, TimeCategory::queue);
        break;
      case 2:
        do_action(Action::print_time, TimeCategory::run);
        break;
      case 3:
        do_action(Action::print_time, TimeCategory::total);
        break;
      }
    case 0:
      if (std::string{"db-path"} == long_options[option_index].name) {
        std::cout << (get_tmp() / db_name).string() << std::endl;
        exit(EXIT_SUCCESS);
      }
      if (std::string{"gh-summary"} == long_options[option_index].name) {
        do_action(Action::github_summary);
      }
      if (std::string{"list-failed"} == long_options[option_index].name) {
        do_action(Action::list, ListCategory::failed);
      }
      if (std::string{"list-queued"} == long_options[option_index].name) {
        do_action(Action::list, ListCategory::queued);
      }
      if (std::string{"list-running"} == long_options[option_index].name) {
        do_action(Action::list, ListCategory::running);
      }
      break;
    case 1:
      do_action(Action::print_time, TimeCategory::queue, std::stoul(optarg));
      break;
    case 2:
      do_action(Action::print_time, TimeCategory::run, std::stoul(optarg));
      break;
    case 3:
      do_action(Action::print_time, TimeCategory::total, std::stoul(optarg));
      break;
    }
  };
}
int32_t Config::get_int(std::string key) { return int_vars[key]; };
std::string Config::get_string(std::string key) { return str_vars[key]; };
bool Config::get_bool(std::string key) { return bool_vars[key]; };
} // namespace tsp