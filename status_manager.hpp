#pragma once
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sqlite3.h>
#include <string>

#include "functions.hpp"
#include "run_cmd.hpp"

namespace tsp {
const std::string db_name("tsp_db.sqlite3");
const std::string db_initialise(
    // Ensure foreign keys are respected
    "PRAGMA foreign_keys = ON;"
    // Create command table
    "CREATE TABLE IF NOT EXISTS jobs (id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "uuid TEXT, command TEXT, category TEXT, slots INTEGER);"
    // Create queue time table
    "CREATE TABLE IF NOT EXISTS qtime (id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "jobid INTEGER NOT NULL, time TIMESTAMP DEFAULT CURRENT_TIMESTAMP, FOREIGN "
    "KEY(jobid) REFERENCES jobs(id) ON DELETE CASCADE);"
    // Create start time table
    "CREATE TABLE IF NOT EXISTS stime (id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "jobid INTEGER NOT NULL, time TIMESTAMP DEFAULT CURRENT_TIMESTAMP, FOREIGN "
    "KEY(jobid) REFERENCES jobs(id) ON DELETE CASCADE);"
    // Create end time table
    "CREATE TABLE IF NOT EXISTS etime (id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "jobid INTEGER NOT NULL, exit_status INTEGER, time TIMESTAMP DEFAULT "
    "CURRENT_TIMESTAMP, FOREIGN KEY(jobid) REFERENCES jobs(id) ON DELETE "
    "CASCADE);"
    // Create stdout/stderr table
    "CREATE TABLE IF NOT EXISTS job_output ( jobid INTEGER UNIQUE NOT NULL, "
    "stdout TEXT, stderr TEXT, FOREIGN KEY(jobid) REFERENCES jobs(id) ON "
    "DELETE CASCADE);"
    // Create the job_times view
    "CREATE VIEW IF NOT EXISTS job_times AS SELECT jobs.id AS "
    "id,command,slots,qtime.time AS qtime,stime.time AS stime,etime.time AS "
    "etime FROM jobs LEFT JOIN qtime ON jobs.id=qtime.jobid LEFT JOIN stime ON "
    "jobs.id=stime.jobid LEFT JOIN etime ON jobs.id=etime.jobid;"
    // Create used_slots view
    "CREATE VIEW IF NOT EXISTS used_slots AS SELECT IFNULL(SUM(slots),0) FROM "
    "job_times WHERE stime IS NOT NULL AND etime IS NULL;"
    // Clean old entries
);
const std::string clean_jobs("DELETE * FROM jobs;");
class Status_Manager {
public:
  const std::string jobid;
  std::ofstream stat_file;
  Status_Manager();
  ~Status_Manager();
  void add_cmd(Run_cmd cmd, std::string category, uint32_t slots);
  void job_start();
  void job_end(int exit_stat);
  void save_output(const std::pair<std::string, std::string> &in);
  bool allowed_to_run();
  bool started;

private:
  std::string
  time_writer(std::chrono::time_point<std::chrono::system_clock> in);
  std::string gen_jobid();
  sqlite3 *conn;
  static int slots_callback(void *ignore2, int ncols, char **out, char **cols);
  char *sqlite_err;
  int sqlite_ret;
  uint32_t slots_req;
  uint32_t slots_used;
  const uint32_t total_slots;
};
} // namespace tsp