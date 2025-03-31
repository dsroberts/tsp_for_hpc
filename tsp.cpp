#include <cstdint>
#include <format>
#include <iostream>
#include <map>
#include <optional>
#include <string>

#include <getopt.h>
#include <unistd.h>

#include "functions.hpp"
#include "help.hpp"
#include "spooler.hpp"
#include "status_manager.hpp"
#include "status_writing.hpp"

namespace tsp {

enum class TSPProgram { spooler, timeout };

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

} // namespace tsp

int main(int argc, char *argv[]) {

  auto prog = tsp::TSPProgram::spooler;

  if (argc == 1) {
    return tsp::do_writer_action(tsp::Action::list, tsp::ListCategory::all);
  }

  auto sp_conf = tsp::Spooler_config();

  opterr = 0;
  int c;
  int option_index;
  while ((c = getopt_long(argc, argv, "+nfL:N:Ei:lho:e:r:v", tsp::long_options,
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
      return tsp::do_writer_action(tsp::Action::info, std::stoul(optarg));
      break;
    case 'o':
      return tsp::do_writer_action(tsp::Action::stdout, std::stoul(optarg));
      break;
    case 'e':
      return tsp::do_writer_action(tsp::Action::stderr, std::stoul(optarg));
      break;
    case 'l':
      return tsp::do_writer_action(tsp::Action::list, tsp::ListCategory::all);
      break;
    case 'h':
      std::cout << std::format(tsp::help, argv[0]) << std::endl;
      return EXIT_SUCCESS;
      break;
    case '?':
      switch (optopt) {
      case 'i':
        return tsp::do_writer_action(tsp::Action::info);
        break;
      case 'o':
        return tsp::do_writer_action(tsp::Action::stdout);
        break;
      case 'e':
        return tsp::do_writer_action(tsp::Action::stderr);
        break;
      case 1:
        return tsp::do_writer_action(tsp::Action::print_time,
                                     tsp::TimeCategory::queue);
        break;
      case 2:
        return tsp::do_writer_action(tsp::Action::print_time,
                                     tsp::TimeCategory::run);
        break;
      case 3:
        return tsp::do_writer_action(tsp::Action::print_time,
                                     tsp::TimeCategory::total);
        break;
      default:
        std::cout << "Unknown option: " << argv[optind - 1] << std::endl;
        std::cout << std::format(tsp::help, argv[0]) << std::endl;
        return EXIT_FAILURE;
        break;
      }
    case 0:
      if (std::string{"db-path"} == tsp::long_options[option_index].name) {
        std::cout << (get_tmp() / tsp::db_name).string() << std::endl;
        return EXIT_SUCCESS;
      }
      if (std::string{"gh-summary"} == tsp::long_options[option_index].name) {
        return tsp::do_writer_action(tsp::Action::github_summary);
      }
      if (std::string{"list-failed"} == tsp::long_options[option_index].name) {
        return tsp::do_writer_action(tsp::Action::list,
                                     tsp::ListCategory::failed);
      }
      if (std::string{"list-queued"} == tsp::long_options[option_index].name) {
        return tsp::do_writer_action(tsp::Action::list,
                                     tsp::ListCategory::queued);
      }
      if (std::string{"list-running"} == tsp::long_options[option_index].name) {
        return tsp::do_writer_action(tsp::Action::list,
                                     tsp::ListCategory::running);
      }
      if (std::string{"list-finished"} == tsp::long_options[option_index].name) {
        return tsp::do_writer_action(tsp::Action::list,
                                     tsp::ListCategory::finished);
      }
      break;
    case 1:
      return tsp::do_writer_action(tsp::Action::print_time,
                                   tsp::TimeCategory::queue,
                                   std::stoul(optarg));
      break;
    case 2:
      return tsp::do_writer_action(tsp::Action::print_time,
                                   tsp::TimeCategory::run, std::stoul(optarg));
      break;
    case 3:
      return tsp::do_writer_action(tsp::Action::print_time,
                                   tsp::TimeCategory::total,
                                   std::stoul(optarg));
      break;
    }
  };
  
  switch(prog) {
  case tsp::TSPProgram::spooler:
    return do_spooler(sp_conf, argc, optind, argv);
    break;
  case tsp::TSPProgram::timeout:
    std::cout << "Soon..." << std::endl;
    return EXIT_SUCCESS;
    break;
  }
  
}