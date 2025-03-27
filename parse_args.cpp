#include "parse_args.hpp"

#include <cstdint>
#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <string>

#include <getopt.h>
#include <unistd.h>

#include "functions.hpp"
#include "spooler.hpp"
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
    {"list-finished", no_argument, nullptr, 0},
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
int parse_args(int argc, char *argv[]) {

  if (argc == 1) {
    return do_writer_action(Action::list, ListCategory::all);
  }

  auto sp_conf = tsp::Spooler_config();

  opterr = 0;
  int c;
  int option_index;
  while ((c = getopt_long(argc, argv, "+nfL:N:Ei:lho:e:r:v", long_options,
                          &option_index)) != -1) {
    switch (c) {
    case 'n':
      // Pipe stdout and stderr of forked process to /dev/null
      sp_conf.set_bool("disappear_output", true);
      break;
    case 'f':
      sp_conf.set_bool("do_fork", false);
      break;
    case 'N':
      sp_conf.set_int("nslots", std::stoul(optarg));
      break;
    case 'E':
      sp_conf.set_bool("separate_stderr", true);
      break;
    case 'L':
      sp_conf.set_string("category", {optarg});
      break;
    case 'v':
      sp_conf.set_bool("verbose", true);
      break;
    case 'r':
      sp_conf.set_int("rerun", std::stoul(optarg));
      break;
    case 'i':
      return do_writer_action(Action::info, std::stoul(optarg));
      break;
    case 'o':
      return do_writer_action(Action::stdout, std::stoul(optarg));
      break;
    case 'e':
      return do_writer_action(Action::stderr, std::stoul(optarg));
      break;
    case 'l':
      return do_writer_action(Action::list, ListCategory::all);
      break;
    case 'h':
      std::cout << std::format(help, argv[0]) << std::endl;
      return EXIT_SUCCESS;
      break;
    case '?':
      switch (optopt) {
      case 'i':
        return do_writer_action(Action::info);
        break;
      case 'o':
        return do_writer_action(Action::stdout);
        break;
      case 'e':
        return do_writer_action(Action::stderr);
        break;
      case 1:
        return do_writer_action(Action::print_time, TimeCategory::queue);
        break;
      case 2:
        return do_writer_action(Action::print_time, TimeCategory::run);
        break;
      case 3:
        return do_writer_action(Action::print_time, TimeCategory::total);
        break;
      default:
        std::cout << "Unknown option: " << argv[optind - 1] << std::endl;
        std::cout << std::format(help, argv[0]) << std::endl;
        return EXIT_FAILURE;
        break;
      }
    case 0:
      if (std::string{"db-path"} == long_options[option_index].name) {
        std::cout << (get_tmp() / db_name).string() << std::endl;
        return EXIT_SUCCESS;
      }
      if (std::string{"gh-summary"} == long_options[option_index].name) {
        return do_writer_action(Action::github_summary);
      }
      if (std::string{"list-failed"} == long_options[option_index].name) {
        return do_writer_action(Action::list, ListCategory::failed);
      }
      if (std::string{"list-queued"} == long_options[option_index].name) {
        return do_writer_action(Action::list, ListCategory::queued);
      }
      if (std::string{"list-running"} == long_options[option_index].name) {
        return do_writer_action(Action::list, ListCategory::running);
      }
      if (std::string{"list-finished"} == long_options[option_index].name) {
        return do_writer_action(Action::list, ListCategory::finished);
      }
      break;
    case 1:
      return do_writer_action(Action::print_time, TimeCategory::queue,
                              std::stoul(optarg));
      break;
    case 2:
      return do_writer_action(Action::print_time, TimeCategory::run,
                              std::stoul(optarg));
      break;
    case 3:
      return do_writer_action(Action::print_time, TimeCategory::total,
                              std::stoul(optarg));
      break;
    }
  };
  // If we make it this far, we haven't been told to do anything else
  return do_spooler(sp_conf, argc, optind, argv);
}
} // namespace tsp