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
    "uuid TEXT, command TEXT, category TEXT, pid INTEGER, slots INTEGER);"
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
    "CREATE VIEW IF NOT EXISTS used_slots AS SELECT IFNULL(SUM(slots),0) as s "
    "FROM job_times WHERE stime IS NOT NULL AND etime IS NULL;"
    // Create sibling_pids view
    "CREATE VIEW IF NOT EXISTS sibling_pids AS SELECT pid FROM jobs WHERE id "
    "IN ( SELECT id FROM job_times WHERE stime IS NOT NULL and etime IS NULL);"
    // Clean old entries
);
const std::string clean_jobs("DELETE FROM jobs;");
class Status_Manager {
public:
  const std::string jobid;
  Status_Manager();
  ~Status_Manager();
  void add_cmd(Run_cmd cmd, std::string category, uint32_t slots);
  void job_start();
  void job_end(int exit_stat);
  void save_output(const std::pair<std::string, std::string> &in);
  std::vector<pid_t> get_running_job_pids();
  bool allowed_to_run();

private:
  sqlite3 *conn_;
  uint32_t slots_req_;
  const uint32_t total_slots_;
  std::string gen_jobid();
};
} // namespace tsp