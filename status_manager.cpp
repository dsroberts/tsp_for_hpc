#include "status_manager.hpp"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <sqlite3.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

#include "functions.hpp"
#include "run_cmd.hpp"
#include "sqlite_statement_manager.hpp"

namespace tsp {

Status_Manager::Status_Manager(bool rw, bool die_on_open_fail)
    : jobid(rw ? gen_jobid() : ""), rw_(rw),
      die_on_open_fail_(die_on_open_fail), total_slots_(0l), slots_set_(false),
      started_(false), finished_(false), pid_(getpid()) {
  if (rw && !die_on_open_fail) {
    die_with_err(
        "Not allowed to continue through open failure when in read-write mode",
        -1);
  }
  open_db();
}
Status_Manager::Status_Manager(bool rw) : Status_Manager(rw, true) {};
Status_Manager::Status_Manager() : Status_Manager(true, true) {};
Status_Manager::~Status_Manager() {
  if (conn_) {
    sqlite3_close_v2(conn_);
  }
}

void Status_Manager::set_total_slots(int32_t total_slots) {
  // Only allow this call once
  if (slots_set_) {
    die_with_err(
        "Error! Attempted to set total number of available cores twice!", -1);
  }
  total_slots_ = total_slots;
  slots_set_ = true;
}

void Status_Manager::open_db() {
  auto stat_fn = get_tmp() / db_name;
  int sqlite_ret;
  db_open_flags_ = SQLITE_OPEN_FULLMUTEX;
  if (rw_) {
    db_open_flags_ |= (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
  } else {
    db_open_flags_ |= SQLITE_OPEN_READONLY;
  }
  if ((sqlite_ret = sqlite3_open_v2(stat_fn.c_str(), &conn_, db_open_flags_,
                                    nullptr)) != SQLITE_OK) {
    if (die_on_open_fail_) {
      die_with_err("Unable to open database", sqlite_ret);
    } else {
      conn_ = nullptr;
      return;
    }
  }
  // Wait a long time if we have to
  if ((sqlite_ret = sqlite3_busy_timeout(conn_, 10000)) != SQLITE_OK) {
    die_with_err("Unable to set busy timeout", sqlite_ret);
  }
  char *sqlite_err;
  // Use exec here as db_initialise contains many statements.
  if ((sqlite_ret = sqlite3_exec(conn_, db_initialise.data(), nullptr, nullptr,
                                 &sqlite_err)) != SQLITE_OK) {
    exit_with_sqlite_err(sqlite_err, db_initialise, sqlite_ret, conn_);
  }
}

void Status_Manager::add_cmd(Run_cmd &cmd, std::string category,
                             int32_t slots) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  slots_req_ = slots;
  {
    auto ssm = Sqlite_statement_manager(conn_, insert_cmd_stmt);
    auto cmd_str = cmd.print();
    auto cmd_vec = cmd.get();
    ssm.step_put(jobid, cmd_str, cmd_vec, category, pid_, slots_req_);
  }
  qtime = now();
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(insert_qtime_stmt, qtime, jobid), true);
}

void Status_Manager::add_cmd(Run_cmd &cmd, uint32_t id) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  std::string category;
  {
    auto ssm =
        Sqlite_statement_manager(conn_, std::format(get_job_category_stmt, id));
    ssm.step_get(true, category, slots_req_);
  }
  {
    auto ssm = Sqlite_statement_manager(conn_, insert_cmd_stmt);
    auto cmd_str = cmd.print();
    auto cmd_vec = cmd.get();
    ssm.step_put(jobid, cmd_str, cmd_vec, category, pid_, slots_req_);
  }
  qtime = now();
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(insert_qtime_stmt, qtime, jobid), true);
}

void Status_Manager::job_start() {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  stime = now();
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(insert_stime_stmt, stime, jobid), true);
  started_ = true;
}

void Status_Manager::job_end(int exit_stat) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  if (!started_) {
    job_start();
  }
  etime = now();
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(insert_etime_stmt, exit_stat, etime, jobid), true);
  finished_ = true;
}

void Status_Manager::save_output(
    const std::pair<std::string, std::string> &in) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  auto ssm = Sqlite_statement_manager(conn_, insert_output_stmt);
  ssm.step_put(jobid, in.first, in.second);
}

void Status_Manager::store_state(prog_state ps) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  auto ssm = Sqlite_statement_manager(conn_, insert_start_state_stmt);
  ssm.step_put(jobid, ps.wd, ps.env.first);
}

/*
Read-only functions
*/
bool Status_Manager::db_not_openable() {
  if (!conn_) {
    open_db();
    return !conn_;
  }
  return false;
}

bool Status_Manager::allowed_to_run() {
  if (db_not_openable()) {
    return {};
  }
  int32_t slots_used;
  auto ssm = Sqlite_statement_manager(conn_, get_used_slots_stmt);
  ssm.step_get(true, slots_used);
  return (total_slots_ - slots_used) >= slots_req_;
}

std::vector<pid_t> Status_Manager::get_running_job_pids(pid_t excl) {
  if (db_not_openable()) {
    return {};
  }
  std::vector<pid_t> out;
  auto ssm =
      Sqlite_statement_manager(conn_, std::format(get_sibling_pids_stmt, excl));
  pid_t tmp;
  while (ssm.step_get(tmp)) {
    out.push_back(tmp);
  }
  return out;
}

uint32_t Status_Manager::get_last_job_id() {
  if (db_not_openable()) {
    return {};
  }
  uint32_t out;
  auto ssm = Sqlite_statement_manager(conn_, get_last_jobid_stmt);
  ssm.step_get(true, out);
  return out;
}

job_stat Status_Manager::get_job_by_id(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  auto ssm =
      Sqlite_statement_manager(conn_, std::format(get_job_by_id_stmt, id));
  job_stat out;
  ssm.step_get(true, out.id, out.cmd, out.category, out.qtime, out.stime,
               out.etime, out.status);
  return out;
}

std::vector<job_stat>
Status_Manager::get_job_stats_by_category(ListCategory c) {
  if (db_not_openable()) {
    return {};
  }
  std::string_view stmt;
  switch (c) {
  case ListCategory::none: // invalid
    die_with_err("Error! Requested a list but no valid list category provided",
                 -1);
    break;
  case ListCategory::all: // all
    stmt = get_all_jobs_stmt;
    break;
  case ListCategory::failed: // failed
    stmt = get_failed_jobs_stmt;
    break;
  case ListCategory::queued: // queued
    stmt = get_queued_jobs_stmt;
    break;
  case ListCategory::running: // running
    stmt = get_running_jobs_stmt;
    break;
  case ListCategory::finished: // finished
    stmt = get_finished_jobs_stmt;
    break;
  }
  auto ssm = Sqlite_statement_manager(conn_, stmt);
  std::vector<job_stat> out;
  job_stat tmp_stat;
  while (ssm.step_get(tmp_stat.id, tmp_stat.cmd, tmp_stat.category,
                      tmp_stat.qtime, tmp_stat.stime, tmp_stat.etime,
                      tmp_stat.status)) {

    out.push_back(tmp_stat);
  }
  return out;
}

job_details Status_Manager::get_job_details_by_id(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(get_job_details_by_id_stmt, id));
  job_details out;
  ssm.step_get(true, out.stat.id, out.stat.cmd, out.stat.category,
               out.stat.qtime, out.stat.stime, out.stat.etime, out.stat.status,
               out.uuid, out.slots, out.pid);

  return out;
}

std::string Status_Manager::get_job_stdout(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(get_job_output_stmt, "stdout", id));
  std::string out;
  ssm.step_get(true, out);
  return out;
}

std::string Status_Manager::get_job_stderr(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(get_job_output_stmt, "stderr", id));
  std::string out;
  ssm.step_get(true, out);
  return out;
}

std::string Status_Manager::get_cmd_to_rerun(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  auto ssm =
      Sqlite_statement_manager(conn_, std::format(get_cmd_to_rerun_stmt, id));
  ptr_array_w_buffer_t out;
  ssm.step_get(true, out);
  return out.second;
}

uint32_t Status_Manager::get_extern_jobid() {
  if (db_not_openable()) {
    return {};
  }
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(get_extern_jobid_stmt, jobid));
  uint32_t out;
  ssm.step_get(true, out);
  return out;
}

prog_state Status_Manager::get_state(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  auto ssm = Sqlite_statement_manager(conn_, std::format(get_state_stmt, id));
  prog_state out;
  ssm.step_get(true, out.wd, out.env);
  return out;
}

std::string Status_Manager::gen_jobid() {
  // https://stackoverflow.com/questions/24365331/how-can-i-generate-uuid-in-c-without-using-boost-library
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, 15);
  static std::uniform_int_distribution<> dis2(8, 11);

  std::stringstream ss;
  int i;
  ss << std::hex;
  for (i = 0; i < 8; i++) {
    ss << dis(gen);
  }
  ss << "-";
  for (i = 0; i < 4; i++) {
    ss << dis(gen);
  }
  ss << "-4";
  for (i = 0; i < 3; i++) {
    ss << dis(gen);
  }
  ss << "-";
  ss << dis2(gen);
  for (i = 0; i < 3; i++) {
    ss << dis(gen);
  }
  ss << "-";
  for (i = 0; i < 12; i++) {
    ss << dis(gen);
  }
  return ss.str();
}
} // namespace tsp