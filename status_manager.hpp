#pragma once
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "functions.hpp"
#include "run_cmd.hpp"

namespace tsp {
constexpr std::string_view db_name("tsp_db.sqlite3");
constexpr std::string_view db_initialise(
    // Ensure foreign keys are respected
    "PRAGMA foreign_keys = ON;"
    // Create command table
    "CREATE TABLE IF NOT EXISTS jobs (id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "uuid TEXT UNIQUE, command TEXT, command_raw BLOB, category TEXT, pid "
    "INTEGER, slots INTEGER);"
    // Create queue time table
    "CREATE TABLE IF NOT EXISTS qtime (id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "jobid INTEGER NOT NULL, time INTEGER, FOREIGN KEY(jobid) REFERENCES "
    "jobs(id) ON DELETE CASCADE);"
    // Create start time table
    "CREATE TABLE IF NOT EXISTS stime (id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "jobid INTEGER NOT NULL, time INTEGER, FOREIGN KEY(jobid) REFERENCES "
    "jobs(id) ON DELETE CASCADE);"
    // Create start_state table
    "CREATE TABLE IF NOT EXISTS start_state (jobid INTEGER UNIQUE NOT NULL, "
    "cwd TEXT, environ BLOB, FOREIGN KEY(jobid) REFERENCES jobs(id) ON DELETE "
    "CASCADE);"
    // Create end time table
    "CREATE TABLE IF NOT EXISTS etime (id INTEGER PRIMARY KEY AUTOINCREMENT,  "
    "jobid INTEGER NOT NULL, exit_status INTEGER, time INTEGER, FOREIGN "
    "KEY(jobid) REFERENCES jobs(id) ON DELETE CASCADE);"
    // Create stdout/stderr table
    "CREATE TABLE IF NOT EXISTS job_output ( jobid INTEGER UNIQUE NOT NULL, "
    "stdout TEXT, stderr TEXT, FOREIGN KEY(jobid) REFERENCES jobs(id) ON "
    "DELETE CASCADE);"
    // Create integer_sequence table
    "CREATE TABLE IF NOT EXISTS integer_sequence( slot INTEGER UNIQUE );"
    // Create used_slots table
    "CREATE TABLE IF NOT EXISTS used_slots( uuid TEXT NOT NULL, slot INTEGER, "
    "FOREIGN KEY(uuid) REFERENCES jobs(uuid) ON DELETE CASCADE);"
    // Create job_details view
    "CREATE VIEW IF NOT EXISTS job_details AS SELECT jobs.id AS "
    "id,uuid,command,category,pid,slots,qtime.time AS qtime,"
    "stime.time AS stime,etime.time AS etime,etime.exit_status "
    "AS exit_status FROM jobs LEFT JOIN qtime ON "
    "jobs.id=qtime.jobid LEFT JOIN stime ON jobs.id=stime.jobid "
    "LEFT JOIN etime ON jobs.id=etime.jobid;"
    // Create slots_in_use view
    "CREATE VIEW IF NOT EXISTS slots_in_use AS SELECT used_slots.uuid AS "
    "uuid,slot FROM jobs LEFT JOIN used_slots ON jobs.uuid = used_slots.uuid "
    "LEFT JOIN etime ON jobs.id = etime.jobid WHERE etime.time IS NULL;"
    // Create sibling_pids view
    "CREATE VIEW IF NOT EXISTS sibling_pids AS SELECT id,pid FROM jobs WHERE "
    "id IN ( SELECT id FROM job_details WHERE stime IS NOT NULL and etime IS "
    "NULL);");

// Clean old entries
constexpr std::string_view clean(
    // Ensure foreign keys are respected
    "PRAGMA foreign_keys = ON; "
    // Remove all jobs
    "DELETE FROM jobs; "
    // Reset sequences
    "DELETE FROM integer_sequence; "
    "DELETE FROM sqlite_sequence;");

constexpr std::string_view create_integer_sequence_stmt(
    "WITH RECURSIVE generate_series(value) AS ( SELECT 0 UNION ALL SELECT "
    "value+1 FROM generate_series WHERE value+1 < ? ) INSERT OR IGNORE INTO "
    "integer_sequence SELECT value FROM generate_series;");

constexpr std::string_view insert_cmd_stmt(
    "INSERT INTO jobs(uuid,command,command_raw,category,pid,slots) VALUES "
    "(?,?,?,?,?,?);");

constexpr std::string_view insert_proc_allocation_stmt(
    "WITH avail_slots AS ( SELECT seq.slot FROM integer_sequence seq LEFT JOIN "
    "slots_in_use si ON seq.slot = si.slot WHERE si.slot IS NULL AND seq.slot "
    "< ?) INSERT INTO used_slots(uuid,slot) SELECT ?,slot FROM avail_slots "
    "WHERE ( SELECT COUNT(*) FROM avail_slots ) > ? ORDER BY slot ASC LIMIT "
    "?;");

constexpr std::string_view recover_proc_allocation_stmt(
    "SELECT slot FROM slots_in_use WHERE uuid = ?;");

constexpr std::string_view insert_qtime_stmt(
    "INSERT INTO qtime(jobid,time) SELECT id,? FROM jobs WHERE uuid = ?;");

constexpr std::string_view insert_stime_stmt(
    "INSERT INTO stime(jobid,time) SELECT id,? FROM jobs WHERE uuid = ?;");

constexpr std::string_view
    insert_etime_stmt("INSERT INTO etime(jobid,exit_status,time) SELECT "
                      "id,?,? FROM jobs WHERE uuid = ?;");

constexpr std::string_view insert_output_stmt(
    "INSERT INTO job_output(jobid,stdout,stderr) VALUES (( SELECT id "
    "FROM jobs WHERE uuid = ? ),?,?);");

constexpr std::string_view insert_start_state_stmt(
    "INSERT INTO start_state(jobid,cwd,environ) VALUES (( SELECT id FROM jobs "
    "WHERE uuid = ? ),?,?);");

constexpr std::string_view
    get_job_category_stmt("SELECT category,slots FROM jobs WHERE id = ?;");

constexpr std::string_view
    get_sibling_pids_stmt("SELECT pid FROM sibling_pids WHERE pid != ?;");

constexpr std::string_view
    get_last_jobid_stmt("SELECT jobs.id FROM jobs LEFT JOIN qtime ON "
                        "jobid = jobs.id ORDER BY time DESC LIMIT 1;");

constexpr std::string_view get_job_by_id_stmt(
    "SELECT id,command,category,qtime,stime,etime,exit_status "
    "FROM job_details WHERE id = ?;");

constexpr std::string_view get_all_jobs_stmt(
    "SELECT id,command,category,qtime,stime,etime,exit_status "
    "FROM job_details;");

constexpr std::string_view get_failed_jobs_stmt(
    "SELECT id,command,category,qtime,stime,etime,exit_status "
    "FROM job_details WHERE exit_status IS NOT NULL AND exit_status != 0;");

constexpr std::string_view get_queued_jobs_stmt(
    "SELECT id,command,category,qtime,stime,etime,exit_status "
    "FROM job_details WHERE stime IS NULL;");

constexpr std::string_view get_finished_jobs_stmt(
    "SELECT id,command,category,qtime,stime,etime,exit_status "
    "FROM job_details WHERE exit_status IS NOT NULL;");

constexpr std::string_view get_running_jobs_stmt(
    "SELECT id,command,category,qtime,stime,etime,exit_status "
    "FROM job_details WHERE stime IS NOT NULL AND etime IS NULL;");

constexpr std::string_view get_job_details_by_id_stmt(
    "SELECT id,command,category,qtime,stime,etime,exit_status,uuid,slots,pid "
    "FROM job_details WHERE id = ?;");

constexpr std::string_view
    has_memprof("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND "
                "name = 'memprof'");

constexpr std::string_view get_max_rss_stmt(
    "SELECT jobid,MAX(rss) / 1048576.0 FROM memprof GROUP BY jobid;");

constexpr std::string_view
    get_job_stdout_stmt("SELECT stdout FROM job_output WHERE jobid = ?;");

constexpr std::string_view
    get_job_stderr_stmt("SELECT stderr FROM job_output WHERE jobid = ?;");

constexpr std::string_view
    get_cmd_to_rerun_stmt("SELECT command_raw FROM jobs WHERE id = ?;");

constexpr std::string_view
    get_state_stmt("SELECT cwd,environ FROM start_state WHERE jobid = ?;");

constexpr std::string_view
    get_extern_jobid_stmt("SELECT id FROM jobs WHERE uuid = ?;");

constexpr std::string_view get_uuid_stmt("SELECT uuid FROM jobs WHERE id = ?;");

enum class ListCategory { none, all, failed, queued, running, finished };

struct job_stat {
  uint32_t id;
  std::string cmd;
  std::optional<std::string> category;
  int64_t qtime;
  std::optional<int64_t> stime;
  std::optional<int64_t> etime;
  std::optional<int32_t> status;
};

struct job_details {
  uint32_t id;
  std::string cmd;
  std::optional<std::string> category;
  int64_t qtime;
  std::optional<int64_t> stime;
  std::optional<int64_t> etime;
  std::optional<int32_t> status;
  std::string uuid;
  int32_t slots;
  std::optional<uint32_t> pid;
};

typedef std::pair<char **, std::string> ptr_array_w_buffer_t;

struct prog_state {
  std::filesystem::path wd;
  ptr_array_w_buffer_t env;
};

class Status_Manager {
public:
  const std::string jobid;
  Status_Manager(bool rw, bool open_can_fail);
  Status_Manager(bool rw);
  Status_Manager();
  ~Status_Manager();
  void set_total_slots(int32_t total_slots);
  void add_cmd(Run_cmd &cmd, std::string category, int32_t slots);
  void add_cmd(Run_cmd &cmd, uint32_t id);
  void insert_proc_allocation();
  std::vector<uint32_t> recover_proc_allocation();
  void job_start();
  void job_end(int exit_stat);
  void save_output(const std::pair<std::string, std::string> &in);
  std::vector<pid_t> get_running_job_pids(pid_t excl);
  uint32_t get_last_job_id();
  job_stat get_job_by_id(uint32_t id);
  job_details get_job_details_by_id(uint32_t id);
  std::vector<job_stat> get_job_stats_by_category(ListCategory c);
  std::map<uint32_t, double> get_max_rss();
  std::string get_job_stdout(uint32_t id);
  std::string get_job_stderr(uint32_t id);
  uint32_t get_extern_jobid();
  std::string get_job_uuid(uint32_t id);
  std::string get_cmd_to_rerun(uint32_t id);
  prog_state get_state(uint32_t id);
  void store_state(prog_state);
  uint64_t qtime;
  uint64_t stime;
  uint64_t etime;

protected:
  sqlite3 *conn_ = nullptr;
  const bool rw_;

private:
  int db_open_flags_;
  int32_t slots_req_;
  const bool die_on_open_fail_;
  int32_t total_slots_;
  bool slots_set_;
  bool started_;
  bool finished_;
  pid_t pid_;
  std::string gen_jobid();
  void open_db();
  bool db_not_openable();
};
} // namespace tsp