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
    exit_with_sqlite_err(sqlite_err, sqlite_ret, nullptr);
  }
}

void Status_Manager::add_cmd(Run_cmd &cmd, std::string category,
                             int32_t slots) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  slots_req_ = slots;
  Sqlite_statement_manager(conn_, insert_cmd_stmt)
      .step(jobid, cmd.print(), cmd.get(), category, pid_, slots_req_);
  qtime = now();
  Sqlite_statement_manager(conn_, insert_qtime_stmt).step(qtime, jobid);
}

void Status_Manager::add_cmd(Run_cmd &cmd, uint32_t id) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  auto out = Sqlite_statement_manager(conn_, get_job_category_stmt)
                 .fetch_one<std::string, int32_t>(id);
  auto category = std::get<0>(out);
  slots_req_ = std::get<1>(out);
  Sqlite_statement_manager(conn_, insert_cmd_stmt)
      .step(jobid, cmd.print(), cmd.get(), category, pid_, slots_req_);
  qtime = now();
  Sqlite_statement_manager(conn_, insert_qtime_stmt).step(qtime, jobid);
}

void Status_Manager::job_start() {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  stime = now();
  Sqlite_statement_manager(conn_, insert_stime_stmt).step(stime, jobid);
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
  Sqlite_statement_manager(conn_, insert_etime_stmt)
      .step(exit_stat, etime, jobid);
  finished_ = true;
}

void Status_Manager::save_output(
    const std::pair<std::string, std::string> &in) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  Sqlite_statement_manager(conn_, insert_output_stmt)
      .step(jobid, in.first, in.second);
}

void Status_Manager::store_state(prog_state ps) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  Sqlite_statement_manager(conn_, insert_start_state_stmt)
      .step(jobid, ps.wd, ps.env.first);
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
  auto slots_used =
      Sqlite_statement_manager(conn_, get_used_slots_stmt).fetch_one<int32_t>();
  return (total_slots_ - slots_used) >= slots_req_;
}

std::vector<pid_t> Status_Manager::get_running_job_pids(pid_t excl) {
  if (db_not_openable()) {
    return {};
  }
  std::vector<pid_t> out;
  auto ssm = Sqlite_statement_manager(conn_, get_sibling_pids_stmt);
  while (auto tmp = ssm.step<pid_t>(excl)) {
    out.push_back(tmp.value());
  }
  return out;
}

uint32_t Status_Manager::get_last_job_id() {
  if (db_not_openable()) {
    return {};
  }
  return Sqlite_statement_manager(conn_, get_last_jobid_stmt)
      .fetch_one<uint32_t>();
}

job_stat Status_Manager::get_job_by_id(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  return std::make_from_tuple<job_stat>(
      Sqlite_statement_manager(conn_, get_job_by_id_stmt)
          .fetch_one<uint32_t, std::string, std::optional<std::string>, int64_t,
                     std::optional<int64_t>, std::optional<int64_t>,
                     std::optional<int32_t>>(id));
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
  while (auto tmp_stat =
             ssm.step<uint32_t, std::string, std::optional<std::string>,
                      int64_t, std::optional<int64_t>, std::optional<int64_t>,
                      std::optional<int32_t>>()) {
    out.push_back(std::make_from_tuple<job_stat>(tmp_stat.value()));
  }
  return out;
}

job_details Status_Manager::get_job_details_by_id(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  return std::make_from_tuple<job_details>(
      Sqlite_statement_manager(conn_, get_job_details_by_id_stmt)
          .fetch_one<uint32_t, std::string, std::optional<std::string>, int64_t,
                     std::optional<int64_t>, std::optional<int64_t>,
                     std::optional<int32_t>, std::string, int32_t,
                     std::optional<uint32_t>>(id));
}

std::string Status_Manager::get_job_stdout(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  return Sqlite_statement_manager(conn_, get_job_stdout_stmt)
      .step<std::string>(id)
      .value_or(std::string());
}

std::string Status_Manager::get_job_stderr(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  return Sqlite_statement_manager(conn_, get_job_stderr_stmt)
      .step<std::string>(id)
      .value_or(std::string());
}

std::string Status_Manager::get_cmd_to_rerun(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  return Sqlite_statement_manager(conn_, get_cmd_to_rerun_stmt)
      .fetch_one<ptr_array_w_buffer_t>(id)
      .second;
}

uint32_t Status_Manager::get_extern_jobid() {
  if (db_not_openable()) {
    return {};
  }
  return Sqlite_statement_manager(conn_, get_extern_jobid_stmt)
      .fetch_one<uint32_t>(jobid);
}

std::string Status_Manager::get_job_uuid(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  return Sqlite_statement_manager(conn_, get_uuid_stmt)
      .fetch_one<std::string>(id);
}

prog_state Status_Manager::get_state(uint32_t id) {
  if (db_not_openable()) {
    return {};
  }
  return std::make_from_tuple<prog_state>(
      Sqlite_statement_manager(conn_, get_state_stmt)
          .fetch_one<std::filesystem::path, ptr_array_w_buffer_t>(id));
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