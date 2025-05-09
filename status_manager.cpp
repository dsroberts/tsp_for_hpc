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
      started_(false), finished_(false) {
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
  int sqlite_ret;
  {
    auto ssm = Sqlite_statement_manager(
        conn_, "INSERT INTO jobs(uuid,command,command_raw,category,pid,slots) "
               "VALUES (?,?,?,?,?,?)");
    if ((sqlite_ret = sqlite3_bind_text(ssm.stmt, 1, jobid.c_str(), -1,
                                        nullptr)) != SQLITE_OK) {
      die_with_err("Unable bind jobid", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_text(ssm.stmt, 2, cmd.print().c_str(), -1,
                                        SQLITE_TRANSIENT)) != SQLITE_OK) {
      die_with_err("Unable bind command", sqlite_ret);
    }
    auto raw_cmd = cmd.serialise();
    if ((sqlite_ret = sqlite3_bind_blob(ssm.stmt, 3, raw_cmd.data(),
                                        raw_cmd.size(), SQLITE_STATIC)) !=
        SQLITE_OK) {
      die_with_err("Unable bind raw command", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_text(ssm.stmt, 4, category.c_str(), -1,
                                        nullptr)) != SQLITE_OK) {
      die_with_err("Unable bind category", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_int(ssm.stmt, 5, getpid())) != SQLITE_OK) {
      die_with_err("Unable bind pid", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_int(ssm.stmt, 6, slots_req_)) != SQLITE_OK) {
      die_with_err("Unable bind nslots", sqlite_ret);
    }
    ssm.step();
  }
  qtime = now();
  auto ssm = Sqlite_statement_manager(
      conn_,
      std::format("INSERT INTO qtime(jobid,time) SELECT id,{} FROM jobs WHERE "
                  "uuid = \"{}\";",
                  qtime, jobid),
      true);
}

void Status_Manager::add_cmd(Run_cmd &cmd, uint32_t id) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  std::string category;
  {
    auto ssm = Sqlite_statement_manager(
        conn_,
        std::format("SELECT category,slots FROM jobs WHERE id = {}", id));
    ssm.step(true);
    category = std::string{
        reinterpret_cast<const char *>(sqlite3_column_text(ssm.stmt, 0))};
    slots_req_ = sqlite3_column_int(ssm.stmt, 1);
  }
  {
    int sqlite_ret;
    auto ssm = Sqlite_statement_manager(
        conn_, "INSERT INTO jobs(uuid,command,command_raw,category,pid,slots) "
               "VALUES (?,?,?,?,?,?)");
    if ((sqlite_ret = sqlite3_bind_text(ssm.stmt, 1, jobid.c_str(), -1,
                                        nullptr)) != SQLITE_OK) {
      die_with_err("Unable bind jobid", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_text(ssm.stmt, 2, cmd.print().c_str(), -1,
                                        SQLITE_TRANSIENT)) != SQLITE_OK) {
      die_with_err("Unable bind command", sqlite_ret);
    }
    auto raw_cmd = cmd.serialise();
    if ((sqlite_ret = sqlite3_bind_blob(ssm.stmt, 3, raw_cmd.data(),
                                        raw_cmd.size(), SQLITE_STATIC)) !=
        SQLITE_OK) {
      die_with_err("Unable bind raw command", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_text(ssm.stmt, 4, category.c_str(), -1,
                                        nullptr)) != SQLITE_OK) {
      die_with_err("Unable bind category", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_int(ssm.stmt, 5, getpid())) != SQLITE_OK) {
      die_with_err("Unable bind pid", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_int(ssm.stmt, 6, slots_req_)) != SQLITE_OK) {
      die_with_err("Unable bind nslots", sqlite_ret);
    }
    ssm.step();
  }
  qtime = now();
  auto ssm = Sqlite_statement_manager(
      conn_,
      std::format("INSERT INTO qtime(jobid,time) SELECT id,{} FROM jobs "
                  "WHERE uuid = \"{}\";",
                  qtime, jobid),
      true);
}

void Status_Manager::job_start() {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  stime = now();
  auto ssm = Sqlite_statement_manager(
      conn_,
      std::format("INSERT INTO stime(jobid,time) SELECT id,{} FROM jobs "
                  "WHERE uuid = \"{}\";",
                  stime, jobid),
      true);
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
      conn_,
      std::format("INSERT INTO etime(jobid,exit_status,time) SELECT "
                  "id,{},{} FROM jobs WHERE uuid= \"{}\";",
                  exit_stat, etime, jobid),
      true);
  finished_ = true;
}

void Status_Manager::save_output(
    const std::pair<std::string, std::string> &in) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  auto ssm = Sqlite_statement_manager(conn_, insert_output_stmt);
  int sqlite_ret;
  if ((sqlite_ret = sqlite3_bind_text(ssm.stmt, 1, jobid.c_str(), -1,
                                      nullptr)) != SQLITE_OK) {
    die_with_err("Unable bind jobid", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_text(ssm.stmt, 2, in.first.c_str(), -1,
                                      nullptr)) != SQLITE_OK) {
    die_with_err("Unable bind stdout", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_text(ssm.stmt, 3, in.second.c_str(), -1,
                                      nullptr)) != SQLITE_OK) {
    die_with_err("Unable bind stderr", sqlite_ret);
  }
  ssm.step();
}

void Status_Manager::store_state(prog_state ps) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  auto ssm = Sqlite_statement_manager(conn_, insert_start_state_stmt);
  int sqlite_ret;
  if ((sqlite_ret = sqlite3_bind_text(ssm.stmt, 1, jobid.c_str(), -1,
                                      nullptr)) != SQLITE_OK) {
    die_with_err("Unable bind jobid", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_text(ssm.stmt, 2, ps.wd.c_str(), -1,
                                      nullptr)) != SQLITE_OK) {
    die_with_err("Unable to bind working directory", sqlite_ret);
  }
  std::string save_env{};
  for (auto i = 0l; ps.env_ptrs[i] != nullptr; ++i) {
    save_env += ps.env_ptrs[i];
    save_env += '\0';
  }
  save_env += '\0';
  if ((sqlite_ret = sqlite3_bind_blob(ssm.stmt, 3, save_env.data(),
                                      save_env.size(), SQLITE_STATIC)) !=
      SQLITE_OK) {
    die_with_err("Unable bind environment", sqlite_ret);
  }
  ssm.step();
}

/*
Read-only functions
*/
bool Status_Manager::allowed_to_run() {
  if (!conn_) {
    open_db();
    if (!conn_) {
      return {};
    }
  }
  int32_t slots_used;
  auto ssm = Sqlite_statement_manager(conn_, "SELECT s FROM used_slots;");
  ssm.step(true);
  slots_used = sqlite3_column_int(ssm.stmt, 0);
  return (total_slots_ - slots_used) >= slots_req_;
}

std::vector<pid_t> Status_Manager::get_running_job_pids(pid_t excl) {
  std::vector<pid_t> out;
  if (!conn_) {
    open_db();
    if (!conn_) {
      return {};
    }
  }
  auto ssm = Sqlite_statement_manager(
      conn_,
      std::format("SELECT pid FROM sibling_pids WHERE pid != {};", excl));
  while (ssm.step() != SQLITE_DONE) {
    out.push_back(static_cast<pid_t>(sqlite3_column_int(ssm.stmt, 0)));
  }
  return out;
}

uint32_t Status_Manager::get_last_job_id() {
  if (!conn_) {
    open_db();
    if (!conn_) {
      return {};
    }
  }
  auto ssm = Sqlite_statement_manager(conn_, get_last_jobid_stmt);
  ssm.step(true);
  return static_cast<uint32_t>(sqlite3_column_int(ssm.stmt, 0));
}

job_stat Status_Manager::get_job_by_id(uint32_t id) {
  if (!conn_) {
    open_db();
    if (!conn_) {
      return {};
    }
  }
  auto ssm =
      Sqlite_statement_manager(conn_, std::format(get_job_by_id_stmt, id));
  ssm.step(true);
  job_stat out;
  out.id = static_cast<uint32_t>(sqlite3_column_int(ssm.stmt, 0));
  out.cmd = std::string{
      reinterpret_cast<const char *>(sqlite3_column_text(ssm.stmt, 1))};
  auto tmp = sqlite3_column_text(ssm.stmt, 2);
  if (tmp[0] != '\0') {
    out.category.emplace(reinterpret_cast<const char *>(tmp));
  }
  out.qtime = sqlite3_column_int64(ssm.stmt, 3);
  tmp = sqlite3_column_text(ssm.stmt, 4);
  if (!!tmp) {
    out.stime.emplace(sqlite3_column_int64(ssm.stmt, 4));
  }
  tmp = sqlite3_column_text(ssm.stmt, 5);
  if (!!tmp) {
    out.etime.emplace(sqlite3_column_int64(ssm.stmt, 5));
  }
  tmp = sqlite3_column_text(ssm.stmt, 6);
  if (!!tmp) {
    out.status.emplace(sqlite3_column_int(ssm.stmt, 6));
  }

  return out;
}

std::vector<job_stat>
Status_Manager::get_job_stats_by_category(ListCategory c) {
  if (!conn_) {
    open_db();
    if (!conn_) {
      return {};
    }
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
  while (ssm.step() == SQLITE_ROW) {
    job_stat tmp_stat;
    tmp_stat.id = static_cast<uint32_t>(sqlite3_column_int(ssm.stmt, 0));
    tmp_stat.cmd = std::string{
        reinterpret_cast<const char *>(sqlite3_column_text(ssm.stmt, 1))};
    auto tmp = sqlite3_column_text(ssm.stmt, 2);
    if (tmp[0] != '\0') {
      tmp_stat.category.emplace(reinterpret_cast<const char *>(tmp));
    }
    tmp_stat.qtime = sqlite3_column_int64(ssm.stmt, 3);
    tmp = sqlite3_column_text(ssm.stmt, 4);
    if (!!tmp) {
      tmp_stat.stime.emplace(sqlite3_column_int64(ssm.stmt, 4));
    }
    tmp = sqlite3_column_text(ssm.stmt, 5);
    if (!!tmp) {
      tmp_stat.etime.emplace(sqlite3_column_int64(ssm.stmt, 5));
    }
    tmp = sqlite3_column_text(ssm.stmt, 6);
    if (!!tmp) {
      tmp_stat.status.emplace(sqlite3_column_int(ssm.stmt, 6));
    }
    out.push_back(tmp_stat);
  }
  return out;
}

job_details Status_Manager::get_job_details_by_id(uint32_t id) {
  if (!conn_) {
    open_db();
    if (!conn_) {
      return {};
    }
  }
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(get_job_details_by_id_stmt, id));
  ssm.step(true);
  job_details out;
  out.stat.id = static_cast<uint32_t>(sqlite3_column_int(ssm.stmt, 0));
  out.stat.cmd = std::string{
      reinterpret_cast<const char *>(sqlite3_column_text(ssm.stmt, 1))};
  auto tmp = sqlite3_column_text(ssm.stmt, 2);
  if (tmp[0] != '\0') {
    out.stat.category.emplace(reinterpret_cast<const char *>(tmp));
  }
  out.stat.qtime = sqlite3_column_int64(ssm.stmt, 3);
  tmp = sqlite3_column_text(ssm.stmt, 4);
  if (!!tmp) {
    out.stat.stime.emplace(sqlite3_column_int64(ssm.stmt, 4));
  }
  tmp = sqlite3_column_text(ssm.stmt, 5);
  if (!!tmp) {
    out.stat.etime.emplace(sqlite3_column_int64(ssm.stmt, 5));
  }
  tmp = sqlite3_column_text(ssm.stmt, 6);
  if (!!tmp) {
    out.stat.status.emplace(sqlite3_column_int(ssm.stmt, 6));
  }
  out.uuid = std::string{
      reinterpret_cast<const char *>(sqlite3_column_text(ssm.stmt, 7))};
  out.slots = static_cast<uint32_t>(sqlite3_column_int(ssm.stmt, 8));
  out.pid.emplace(sqlite3_column_int(ssm.stmt, 9));

  return out;
}

std::string Status_Manager::get_job_stdout(uint32_t id) {
  if (!conn_) {
    open_db();
    if (!conn_) {
      return {};
    }
  }
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(get_job_output_stmt, "stdout", id));
  ssm.step(true);
  return {reinterpret_cast<const char *>(sqlite3_column_text(ssm.stmt, 0))};
}

std::string Status_Manager::get_job_stderr(uint32_t id) {
  if (!conn_) {
    open_db();
    if (!conn_) {
      return {};
    }
  }
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(get_job_output_stmt, "stderr", id));
  ssm.step(true);
  return {reinterpret_cast<const char *>(sqlite3_column_text(ssm.stmt, 0))};
}

std::string Status_Manager::get_cmd_to_rerun(uint32_t id) {
  if (!conn_) {
    open_db();
    if (!conn_) {
      return {};
    }
  }
  auto ssm =
      Sqlite_statement_manager(conn_, std::format(get_cmd_to_rerun_stmt, id));
  ssm.step(true);
  return {reinterpret_cast<const char *>(sqlite3_column_blob(ssm.stmt, 0)),
          static_cast<size_t>(sqlite3_column_bytes(ssm.stmt, 0))};
}

uint32_t Status_Manager::get_extern_jobid() {
  if (!conn_) {
    open_db();
    if (!conn_) {
      return {};
    }
  }
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(get_extern_jobid_stmt, jobid));
  ssm.step(true);
  return static_cast<uint32_t>(sqlite3_column_int(ssm.stmt, 0));
}

prog_state Status_Manager::get_state(uint32_t id) {
  if (!conn_) {
    open_db();
    if (!conn_) {
      return {};
    }
  }
  auto ssm = Sqlite_statement_manager(conn_, std::format(get_state_stmt, id));
  ssm.step(true);
  auto environ_string = std::string{
      reinterpret_cast<const char *>(sqlite3_column_blob(ssm.stmt, 1)),
      static_cast<size_t>(sqlite3_column_bytes(ssm.stmt, 1))};
  // How many tokens?
  auto ntokens = 0ul;
  for (auto c : environ_string) {
    if (c == '\0') {
      ntokens++;
    }
  }
  // Allocate output array
  char **env_ptrs;
  if (nullptr ==
      (env_ptrs = static_cast<char **>(malloc((ntokens) * sizeof(char *))))) {
    die_with_err_errno("Malloc failed", -1);
  }
  // Walk through the copied environ_string and create a pointer to
  // the start of each token
  auto ctr = 0ul;
  auto start = 0ul;
  auto end = environ_string.find('\0');
  while (ctr < ntokens - 1) {
    env_ptrs[ctr] = &environ_string[start];
    start = end + 1;
    end = environ_string.find('\0', start);
    ctr++;
  }
  env_ptrs[ntokens - 1] = nullptr;
  return {env_ptrs,
          std::filesystem::path{
              reinterpret_cast<const char *>(sqlite3_column_text(ssm.stmt, 0))},
          std::move(environ_string)};
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