#include "config.hpp"

#include <cstdint>
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
    {"list-jobs", no_argument, nullptr, 'l'},
    {"info", required_argument, nullptr, 'i'},
    {"db-path", no_argument, nullptr, 0},
    {"gh-summary", no_argument, nullptr, 0},
    {"help", no_argument, nullptr, 0},
    {nullptr, 0, nullptr, 0}};

namespace tsp {
Config::Config(int argc, char *argv[])
    : bool_vars{{"disappear_output", false},
                {"do_fork", true},
                {"separate_stderr", false}},
      int_vars{{"nslots", 1u}}, str_vars{} {

  int c;
  int option_index;
  while ((c = getopt_long(argc, argv, "+nfL:N:Ei:", long_options,
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
    case 'i':
      do_action(Action::info,std::stoul(optarg));
      break;
    case ':':
      switch(optopt) {
        case 'i':
        do_action(Action::info);
        break;
      }
    case 0:
      if (std::string{"help"} == long_options[option_index].name) {
        std::cout << std::format(help, argv[0]) << std::endl;
        exit(EXIT_SUCCESS);
      }
      if (std::string{"db-path"} == long_options[option_index].name) {
        std::cout << (get_tmp() / db_name).string() << std::endl;
        exit(EXIT_SUCCESS);
      }
      if (std::string{"gh-summary"} == long_options[option_index].name) {
        do_action(Action::github_summary);
      }
      break;
    }
  };
}
uint32_t Config::get_int(std::string key) { return int_vars[key]; };
std::string Config::get_string(std::string key) { return str_vars[key]; };
bool Config::get_bool(std::string key) { return bool_vars[key]; };

void launch_frontend() { auto status_manager = tsp::Status_Manager(false); }
} // namespace tsp