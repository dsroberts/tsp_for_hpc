#include "status_manager.hpp"

#include <chrono>
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

Status_Manager::Status_Manager(bool rw)
    : jobid(rw ? gen_jobid() : ""), total_slots_(rw ? get_cgroup().size() : 0) {
  auto stat_fn = get_tmp() / db_name;
  int sqlite_ret;
  if ((sqlite_ret = sqlite3_open_v2(stat_fn.c_str(), &conn_,
                                    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                        SQLITE_OPEN_FULLMUTEX,
                                    nullptr)) != SQLITE_OK) {
    die_with_err("Unable to open database", sqlite_ret);
  }
  // Wait a long time if we have to
  if ((sqlite_ret = sqlite3_busy_timeout(conn_, 10000)) != SQLITE_OK) {
    die_with_err("Unable to set busy timeout", sqlite_ret);
  }
  if (rw) {
    auto ssm = Sqlite_statement_manager(conn_, db_initialise, true);
  }
}
Status_Manager::Status_Manager() : Status_Manager(true) {};
Status_Manager::~Status_Manager() { sqlite3_close_v2(conn_); }

void Status_Manager::add_cmd(Run_cmd &cmd, std::string category,
                             int32_t slots) {
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

  auto ssm = Sqlite_statement_manager(
      conn_,
      std::format("INSERT INTO qtime(jobid,time) SELECT id,{} FROM jobs WHERE "
                  "uuid = \"{}\";",
                  now(), jobid),
      true);
}

void Status_Manager::add_cmd(Run_cmd &cmd, uint32_t id) {
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
  auto ssm = Sqlite_statement_manager(
      conn_,
      std::format("INSERT INTO qtime(jobid,time) SELECT id,{} FROM jobs "
                  "WHERE uuid = \"{}\";",
                  now(), jobid),
      true);
}

bool Status_Manager::allowed_to_run() {
  int32_t slots_used;
  auto ssm = Sqlite_statement_manager(conn_, "SELECT s FROM used_slots;");
  ssm.step(true);
  slots_used = sqlite3_column_int(ssm.stmt, 0);
  return (total_slots_ - slots_used) >= slots_req_;
}

std::vector<pid_t> Status_Manager::get_running_job_pids(pid_t excl) {
  std::vector<pid_t> out;
  auto ssm = Sqlite_statement_manager(
      conn_,
      std::format("SELECT pid FROM sibling_pids WHERE pid != {};", excl));
  while (ssm.step() != SQLITE_DONE) {
    out.push_back(static_cast<pid_t>(sqlite3_column_int(ssm.stmt, 0)));
  }
  return out;
}

void Status_Manager::job_start() {
  auto ssm = Sqlite_statement_manager(
      conn_,
      std::format("INSERT INTO stime(jobid,time) SELECT id,{} FROM jobs "
                  "WHERE uuid = \"{}\";",
                  now(), jobid),
      true);
}

void Status_Manager::job_end(int exit_stat) {
  auto ssm = Sqlite_statement_manager(
      conn_,
      std::format("INSERT INTO etime(jobid,exit_status,time) SELECT "
                  "id,{},{} FROM jobs WHERE uuid= \"{}\";",
                  exit_stat, now(), jobid),
      true);
}

void Status_Manager::save_output(
    const std::pair<std::string, std::string> &in) {
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

/*
Read-only functions
*/
uint32_t Status_Manager::get_last_job_id() {
  auto ssm = Sqlite_statement_manager(conn_, get_last_jobid_stmt);
  ssm.step(true);
  auto out = static_cast<uint32_t>(sqlite3_column_int(ssm.stmt, 0));
  return out;
}

job_stat Status_Manager::get_job_by_id(uint32_t id) {
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

std::vector<job_stat> Status_Manager::get_all_job_stats() {
  auto ssm = Sqlite_statement_manager(conn_, get_all_jobs_stmt);
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

std::vector<job_stat> Status_Manager::get_failed_job_stats() {
  auto ssm = Sqlite_statement_manager(conn_, get_failed_jobs_stmt);
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
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(get_job_output_stmt, "stdout", id));
  ssm.step(true);
  auto out = std::string{
      reinterpret_cast<const char *>(sqlite3_column_text(ssm.stmt, 0))};
  return out;
}

std::string Status_Manager::get_job_stderr(uint32_t id) {
  auto ssm = Sqlite_statement_manager(
      conn_, std::format(get_job_output_stmt, "stderr", id));
  ssm.step(true);
  auto out = std::string{
      reinterpret_cast<const char *>(sqlite3_column_text(ssm.stmt, 0))};
  return out;
}

std::string Status_Manager::get_cmd_to_rerun(uint32_t id) {
  auto ssm =
      Sqlite_statement_manager(conn_, std::format(get_cmd_to_rerun_stmt, id));
  ssm.step(true);
  std::cout << sqlite3_column_bytes(ssm.stmt, 0) << std::endl;
  auto out = std::string{
      reinterpret_cast<const char *>(sqlite3_column_blob(ssm.stmt, 0)),
      static_cast<size_t>(sqlite3_column_bytes(ssm.stmt, 0))};
  return out;
}

std::pair<char **, std::filesystem::path>
Status_Manager::get_state(uint32_t id) {
  std::pair<char **, std::filesystem::path> out;
  auto ssm = Sqlite_statement_manager(conn_, std::format(get_state_stmt, id));
  ssm.step(true);
  auto environ_string = std::string{
      reinterpret_cast<const char *>(sqlite3_column_blob(ssm.stmt, 0)),
      static_cast<size_t>(sqlite3_column_bytes(ssm.stmt, 0))};

  out.second = std::filesystem::path{
      reinterpret_cast<const char *>(sqlite3_column_text(ssm.stmt, 1))};
  return out;
}
void Status_Manager::store_state(char **env, std::filesystem::path wd) {}

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