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
#include "timeout.hpp"
// Disable memprof on not-linux systems
#ifdef __linux__
#include "memprof.hpp"
#else
#include "generic_config.hpp"
#endif

namespace tsp {

enum class TSPProgram { spooler, writer, timeout, memprof };

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
    {"memprof", no_argument, nullptr, 0},
    {"nobind", no_argument, nullptr, 0},
    {"print-queue-time", required_argument, nullptr, 1},
    {"print-run-time", required_argument, nullptr, 2},
    {"print-total-time", required_argument, nullptr, 3},
    {"stdout", required_argument, nullptr, 'o'},
    {"stderr", required_argument, nullptr, 'e'},
    {"rerun", required_argument, nullptr, 'r'},
    {"verbose", no_argument, nullptr, 'v'},
    {"timeout", no_argument, nullptr, 0},
    {"polling-interval", required_argument, nullptr, 'p'},
    {"idle-timeout", required_argument, nullptr, 'I'},
    {"job-timeout", required_argument, nullptr, 'T'},
    {"help", no_argument, nullptr, 'h'},
    {nullptr, 0, nullptr, 0}};

} // namespace tsp

int main(int argc, char *argv[]) {

  auto prog = tsp::TSPProgram::spooler;

  if (argc == 1) {
    return tsp::do_writer(tsp::Action::list, tsp::TimeCategory::none,
                          tsp::ListCategory::all, {});
  }

  auto sp_conf = tsp::Spooler_config();
  auto timeout_conf = tsp::Timeout_config();
// Disable memprof on not-linux systems
#ifdef __linux__
  auto memprof_conf = tsp::Memprof_config();
#else
  auto memprof_conf = tsp::Generic_config();
#endif
  auto writer_action = tsp::Action::none;
  auto time_cat = tsp::TimeCategory::none;
  auto list_cat = tsp::ListCategory::none;
  std::optional<uint32_t> jobid;

  auto leave_options_loop = false;

  opterr = 0;
  int c;
  int option_index;
  while ((c = getopt_long(argc, argv, "+nfL:N:Ei:lho:e:r:vT:I:p:",
                          tsp::long_options, &option_index)) != -1) {
    switch (c) {
    case 'n':
      // Pipe stdout and stderr of forked process to /dev/null
      sp_conf.set_bool("disappear_output", true);
      break;
    case 'f':
      sp_conf.set_bool("do_fork", false);
      timeout_conf.set_bool("do_fork", false);
      memprof_conf.set_bool("do_fork", false);
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
      timeout_conf.set_bool("verbose", true);
      memprof_conf.set_bool("verbose", true);
      break;
    case 'r':
      sp_conf.set_int("rerun", std::stoul(optarg));
      break;
    case 'i':
      prog = tsp::TSPProgram::writer;
      writer_action = tsp::Action::info;
      jobid = std::stoul(optarg);
      leave_options_loop = true;
      break;
    case 'o':
      prog = tsp::TSPProgram::writer;
      writer_action = tsp::Action::stdout;
      jobid = std::stoul(optarg);
      leave_options_loop = true;
      break;
    case 'e':
      prog = tsp::TSPProgram::writer;
      writer_action = tsp::Action::stderr;
      jobid = std::stoul(optarg);
      leave_options_loop = true;
      break;
    case 'l':
      prog = tsp::TSPProgram::writer;
      writer_action = tsp::Action::list;
      list_cat = tsp::ListCategory::all;
      leave_options_loop = true;
      break;
    case 'p':
      timeout_conf.set_int("polling_interval", std::stoul(optarg));
      memprof_conf.set_int("polling_interval", std::stoul(optarg));
      break;
    case 'I':
      timeout_conf.set_int("idle_timeout", std::stoul(optarg));
      memprof_conf.set_int("idle_timeout", std::stoul(optarg));
      break;
    case 'T':
      timeout_conf.set_int("job_timeout", std::stoul(optarg));
      break;
    case 'h':
      std::cout << std::format(tsp::help, argv[0]) << std::endl;
      return EXIT_SUCCESS;
      break;
    case '?':
      switch (optopt) {
      case 'i':
        prog = tsp::TSPProgram::writer;
        writer_action = tsp::Action::info;
        leave_options_loop = true;
        break;
      case 'o':
        prog = tsp::TSPProgram::writer;
        writer_action = tsp::Action::stdout;
        leave_options_loop = true;
        break;
      case 'e':
        prog = tsp::TSPProgram::writer;
        writer_action = tsp::Action::stderr;
        leave_options_loop = true;
        break;
      case 1:
        prog = tsp::TSPProgram::writer;
        writer_action = tsp::Action::print_time;
        time_cat = tsp::TimeCategory::queue;
        leave_options_loop = true;
        break;
      case 2:
        prog = tsp::TSPProgram::writer;
        writer_action = tsp::Action::print_time;
        time_cat = tsp::TimeCategory::run;
        leave_options_loop = true;
        break;
      case 3:
        prog = tsp::TSPProgram::writer;
        writer_action = tsp::Action::print_time;
        time_cat = tsp::TimeCategory::total;
        leave_options_loop = true;
        break;
      default:
        std::cout << "Unknown option: " << argv[optind - 1] << std::endl;
        std::cout << std::format(tsp::help, argv[0]) << std::endl;
        return EXIT_FAILURE;
        break;
      }
    case 0:
      if (std::string{"db-path"} == tsp::long_options[option_index].name) {
        std::cout << (tsp::get_tmp() / tsp::db_name).string() << std::endl;
        return EXIT_SUCCESS;
      }
      if (std::string{"gh-summary"} == tsp::long_options[option_index].name) {
        prog = tsp::TSPProgram::writer;
        writer_action = tsp::Action::github_summary;
        leave_options_loop = true;
      }
      if (std::string{"list-failed"} == tsp::long_options[option_index].name) {
        prog = tsp::TSPProgram::writer;
        writer_action = tsp::Action::list;
        list_cat = tsp::ListCategory::failed;
        leave_options_loop = true;
      }
      if (std::string{"list-queued"} == tsp::long_options[option_index].name) {
        prog = tsp::TSPProgram::writer;
        writer_action = tsp::Action::list;
        list_cat = tsp::ListCategory::queued;
        leave_options_loop = true;
      }
      if (std::string{"list-running"} == tsp::long_options[option_index].name) {
        prog = tsp::TSPProgram::writer;
        writer_action = tsp::Action::list;
        list_cat = tsp::ListCategory::running;
        leave_options_loop = true;
      }
      if (std::string{"list-finished"} ==
          tsp::long_options[option_index].name) {
        prog = tsp::TSPProgram::writer;
        writer_action = tsp::Action::list;
        list_cat = tsp::ListCategory::finished;
        leave_options_loop = true;
      }
      if (std::string{"timeout"} == tsp::long_options[option_index].name) {
        prog = tsp::TSPProgram::timeout;
      }
      if (std::string{"memprof"} == tsp::long_options[option_index].name) {
        prog = tsp::TSPProgram::memprof;
      }
      if (std::string{"nobind"} == tsp::long_options[option_index].name) {
        sp_conf.set_bool("binding", false);
      }
      break;
    case 1:
      prog = tsp::TSPProgram::writer;
      writer_action = tsp::Action::print_time;
      time_cat = tsp::TimeCategory::queue;
      jobid = std::stoul(optarg);
      leave_options_loop = true;
      break;
    case 2:
      prog = tsp::TSPProgram::writer;
      writer_action = tsp::Action::print_time;
      time_cat = tsp::TimeCategory::run;
      jobid = std::stoul(optarg);
      leave_options_loop = true;
      break;
    case 3:
      prog = tsp::TSPProgram::writer;
      writer_action = tsp::Action::print_time;
      time_cat = tsp::TimeCategory::total;
      jobid = std::stoul(optarg);
      leave_options_loop = true;
      break;
    }
    if (leave_options_loop) {
      break;
    }
  };

  switch (prog) {
  case tsp::TSPProgram::spooler:
    return tsp::do_spooler(sp_conf, argc, optind, argv);
    break;
  case tsp::TSPProgram::writer:
    return tsp::do_writer(writer_action, time_cat, list_cat, jobid);
    break;
  case tsp::TSPProgram::timeout:
    return tsp::do_timeout(timeout_conf);
    break;
  case tsp::TSPProgram::memprof:
// Disable memprof on not-linux systems
#ifdef __linux__
    return tsp::do_memprof(memprof_conf);
#else
    die_with_err("memprof only implemented on linux", -1)
#endif
    break;
  }
}