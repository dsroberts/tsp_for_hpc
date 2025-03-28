#pragma once

#include <cstdint>
#include <map>
#include <string>

#include <getopt.h>
#include <unistd.h>

namespace tsp {

constexpr std::string_view help{
    "  == TSP: Task Spooler for HPC == \n"
    " A topology-aware serverless task spooler. tsp detects other TSP "
    "instances\n"
    " on the same node and determines which CPU cores those instances are "
    "bound\n"
    " to. When sufficient cores are free, TSP will allow its queued task to "
    "run\n"
    " and bind it to the first N available cores. State is managed through an "
    "\n"
    " sqlite3 database which can be queried using the TSP command.\n\n"
    " When 'Job Querying Options' are present, the first such option will\n"
    " be used to determine the output of TSP. If none are present, then TSP\n"
    " will assume it is being used to run a command. The exception to this\n"
    " is when no arguments are given, TSP will act as if '-l' has been\n"
    " passed\n\n"
    "Usage: {} [OPTION]... [COMMAND...] \n\n"
    "Job Submission Options:\n"
    "  -n, --no-output        Do not store stdout/stderr of COMMAND\n"
    "  -f, --no-fork          Do not fork the original TSP process to the "
    "background\n"
    "  -N, --nslots=SLOTS     Number of physical cores required (default is "
    "1)\n"
    "  -E, --separate-stderr  Store stdout and stderr in different files\n"
    "  -L, --label=LABEL      Add a label to the task to facilitate simpler "
    "querying\n"
    "  -v, --verbose          Print status messages when job starts and ends\n"
    "  -r, --rerun=ID         Rerun job with id ID\n\n"
    "Job Querying Options:\n"
    "  -l, --list             Show the job list (default action)\n"
    "      --list-failed      Show the list of failed jobs\n"
    "      --list-running     Show the list of running jobs\n"
    "      --list-queued      Show the list of queued jobs\n"
    "      --print-queue-time=[ID]\n"
    "      --print-run-time=[ID]\n"
    "      --print-total-time=[ID]\n"
    "                         Show the queue/run/total (queue+run) time of job "
    "[ID].\n"
    "                         (latest if ID is omitted)\n"
    "  -i, --info=[ID]        Show detailed info for job [ID] (latest if ID is "
    "omitted)\n"
    "  -o, --stdout=[ID]      Display the output of the job [ID] (latest if ID "
    "\n"
    "                         is omitted)\n"
    "  -e, --stderr=[ID]      If -E was provided, display the error file of \n"
    "                         the job [ID] (latest if ID is omitted)\n"
    "      --db-path          Output the path to the database\n"
    "      --gh-summary       Output summary info for github actions\n\n"
    "Other Options:\n"
    "  -h, --help    display this help and exit\n"};
int parse_args(int argc, char *argv[]);
} // namespace tsp