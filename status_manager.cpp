#include "status_manager.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <sqlite3.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>

#include "functions.hpp"
#include "run_cmd.hpp"

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
    char *sqlite_err;
    if ((sqlite_ret = sqlite3_exec(conn_, db_initialise.data(), nullptr,
                                   nullptr, &sqlite_err)) != SQLITE_OK) {
      die_with_err(std::string("Error initialising database :") + sqlite_err,
                   sqlite_ret);
    }
  }
}
Status_Manager::Status_Manager() : Status_Manager(true) {};
Status_Manager::~Status_Manager() { sqlite3_close_v2(conn_); }

void Status_Manager::add_cmd(Run_cmd &cmd, std::string category,
                             int32_t slots) {
  slots_req_ = slots;
  sqlite3_stmt *stmt;
  int sqlite_ret;
  if ((sqlite_ret = sqlite3_prepare_v2(
           conn_,
           "INSERT INTO jobs(uuid,command,command_raw,category,pid,slots) "
           "VALUES "
           "(?,?,?,?,?,?)",
           -1, &stmt, nullptr)) != SQLITE_OK) {
    die_with_err("Unable to prepare new jobid statement", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_text(stmt, 1, jobid.c_str(), -1, nullptr)) !=
      SQLITE_OK) {
    die_with_err("Unable bind jobid", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_text(stmt, 2, cmd.print().c_str(), -1,
                                      SQLITE_TRANSIENT)) != SQLITE_OK) {
    die_with_err("Unable bind command", sqlite_ret);
  }
  auto raw_cmd = cmd.serialise();
  if ((sqlite_ret = sqlite3_bind_blob(stmt, 3, raw_cmd.data(), raw_cmd.size(),
                                      SQLITE_STATIC)) != SQLITE_OK) {
    die_with_err("Unable bind raw command", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_text(stmt, 4, category.c_str(), -1,
                                      nullptr)) != SQLITE_OK) {
    die_with_err("Unable bind category", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_int(stmt, 5, getpid())) != SQLITE_OK) {
    die_with_err("Unable bind pid", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_int(stmt, 6, slots_req_)) != SQLITE_OK) {
    die_with_err("Unable bind nslots", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_DONE) {
    die_with_err("Unable insert data", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }

  char *sqlite_err;
  if ((sqlite_ret =
           sqlite3_exec(conn_,
                        std::format("INSERT INTO qtime(jobid,time) SELECT "
                                    "id,{} FROM jobs WHERE uuid = \"{}\";",
                                    now(), jobid)
                            .c_str(),
                        nullptr, nullptr, &sqlite_err)) != SQLITE_OK) {
    die_with_err("Unable to prepare new jobid statement", sqlite_ret);
  }
}

void Status_Manager::add_cmd(Run_cmd &cmd, uint32_t id) {
  sqlite3_stmt *stmt;
  int sqlite_ret;

  if ((sqlite_ret = sqlite3_prepare_v2(
           conn_,
           std::format("SELECT category,slots FROM jobs WHERE id = {}", id)
               .c_str(),
           -1, &stmt, nullptr)) != SQLITE_OK) {
    die_with_err("Unable to prepare new jobid statement", sqlite_ret);
  }

  if ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_ROW) {
    die_with_err("No job with requested id exists", sqlite_ret);
  }
  auto category =
      std::string{reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))};
  slots_req_ = sqlite3_column_int(stmt, 1);
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }

  if ((sqlite_ret = sqlite3_prepare_v2(
           conn_,
           "INSERT INTO jobs(uuid,command,command_raw,category,pid,slots) "
           "VALUES "
           "(?,?,?,?,?,?)",
           -1, &stmt, nullptr)) != SQLITE_OK) {
    die_with_err("Unable to prepare new jobid statement", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_text(stmt, 1, jobid.c_str(), -1, nullptr)) !=
      SQLITE_OK) {
    die_with_err("Unable bind jobid", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_text(stmt, 2, cmd.print().c_str(), -1,
                                      SQLITE_TRANSIENT)) != SQLITE_OK) {
    die_with_err("Unable bind command", sqlite_ret);
  }
  auto raw_cmd = cmd.serialise();
  if ((sqlite_ret = sqlite3_bind_blob(stmt, 3, raw_cmd.data(), raw_cmd.size(),
                                      SQLITE_STATIC)) != SQLITE_OK) {
    die_with_err("Unable bind raw command", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_text(stmt, 4, category.c_str(), -1,
                                      nullptr)) != SQLITE_OK) {
    die_with_err("Unable bind category", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_int(stmt, 5, getpid())) != SQLITE_OK) {
    die_with_err("Unable bind pid", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_int(stmt, 6, slots_req_)) != SQLITE_OK) {
    die_with_err("Unable bind nslots", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_DONE) {
    die_with_err("Unable insert data", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }

  char *sqlite_err;
  if ((sqlite_ret =
           sqlite3_exec(conn_,
                        std::format("INSERT INTO qtime(jobid,time) SELECT "
                                    "id,{} FROM jobs WHERE uuid = \"{}\";",
                                    now(), jobid)
                            .c_str(),
                        nullptr, nullptr, &sqlite_err)) != SQLITE_OK) {
    die_with_err("Unable to prepare new jobid statement", sqlite_ret);
  }
}

bool Status_Manager::allowed_to_run() {
  sqlite3_stmt *stmt;
  int sqlite_ret;
  int32_t slots_used;
  if ((sqlite_ret = sqlite3_prepare_v2(conn_, "SELECT s FROM used_slots;", -1,
                                       &stmt, nullptr)) != SQLITE_OK) {
    die_with_err("Unable to prepare get used slots statement", sqlite_ret);
  }
  while ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_DONE) {
    if (sqlite_ret != SQLITE_ROW) {
      die_with_err("Pid row unable to be returned", sqlite_ret);
    }
    slots_used = sqlite3_column_int(stmt, 0);
  }
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }

  return (total_slots_ - slots_used) >= slots_req_;
}

std::vector<pid_t> Status_Manager::get_running_job_pids(pid_t excl) {
  sqlite3_stmt *stmt;
  int sqlite_ret;
  std::vector<pid_t> out;
  if ((sqlite_ret = sqlite3_prepare_v2(
           conn_,
           std::format("SELECT pid FROM sibling_pids WHERE pid != {};", excl)
               .c_str(),
           -1, &stmt, nullptr)) != SQLITE_OK) {
    die_with_err("Unable to prepare get pid statement", sqlite_ret);
  }
  while ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_DONE) {
    if (sqlite_ret != SQLITE_ROW) {
      die_with_err("Pid row unable to be returned", sqlite_ret);
    }
    out.push_back(static_cast<pid_t>(sqlite3_column_int(stmt, 0)));
  }
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }

  return out;
}

void Status_Manager::job_start() {
  int sqlite_ret;
  char *sqlite_err;
  if ((sqlite_ret =
           sqlite3_exec(conn_,
                        std::format("INSERT INTO stime(jobid,time) SELECT "
                                    "id,{} FROM jobs WHERE uuid = \"{}\";",
                                    now(), jobid)
                            .c_str(),
                        nullptr, nullptr, &sqlite_err)) != SQLITE_OK) {
    throw std::runtime_error(
        std::string("Unable to insert stime into database: ") + sqlite_err);
  }
}

void Status_Manager::job_end(int exit_stat) {
  int sqlite_ret;
  char *sqlite_err;
  if ((sqlite_ret = sqlite3_exec(
           conn_,
           std::format("INSERT INTO etime(jobid,exit_status,time) SELECT "
                       "id,{},{} FROM jobs WHERE uuid= \"{}\";",
                       exit_stat, now(), jobid)
               .c_str(),
           nullptr, nullptr, &sqlite_err)) != SQLITE_OK) {
    throw std::runtime_error(
        std::string("Unable to insert etime into database: ") + sqlite_err);
  }
}

void Status_Manager::save_output(
    const std::pair<std::string, std::string> &in) {
  int sqlite_ret;
  sqlite3_stmt *stmt;
  if ((sqlite_ret = sqlite3_prepare_v2(
           conn_,
           "INSERT INTO job_output(jobid,stdout,stderr) VALUES (( SELECT id "
           "FROM jobs WHERE uuid = ? ),?,?)",
           -1, &stmt, nullptr)) != SQLITE_OK) {
    die_with_err("Unable to prepare new output save statement", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_text(stmt, 1, jobid.c_str(), -1, nullptr)) !=
      SQLITE_OK) {
    die_with_err("Unable bind jobid", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_text(stmt, 2, in.first.c_str(), -1,
                                      nullptr)) != SQLITE_OK) {
    die_with_err("Unable bind stdout", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_text(stmt, 3, in.second.c_str(), -1,
                                      nullptr)) != SQLITE_OK) {
    die_with_err("Unable bind stderr", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_DONE) {
    die_with_err("Unable insert data", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }
}

/*
Read-only functions
*/
uint32_t Status_Manager::get_last_job_id() {

  int sqlite_ret;
  sqlite3_stmt *stmt;
  if ((sqlite_ret = sqlite3_prepare_v2(conn_, get_last_jobid_stmt.data(), -1,
                                       &stmt, nullptr) != SQLITE_OK)) {
    die_with_err("Unable to prepare get jobid statement", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_ROW) {
    die_with_err("Unable get latest jobid", sqlite_ret);
  }
  auto out = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }
  return out;
}

job_stat Status_Manager::get_job_by_id(uint32_t id) {
  int sqlite_ret;
  sqlite3_stmt *stmt;

  if ((sqlite_ret = sqlite3_prepare_v2(
                        conn_, std::format(get_job_by_id_stmt, id).c_str(), -1,
                        &stmt, nullptr) != SQLITE_OK)) {
    die_with_err("Unable to prepare get jobid statement", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_ROW) {
    die_with_err("Unable get job stats", sqlite_ret);
  }
  job_stat out;
  out.id = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
  out.cmd =
      std::string{reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1))};
  auto tmp = sqlite3_column_text(stmt, 2);
  if (tmp[0] != '\0') {
    out.category.emplace(reinterpret_cast<const char *>(tmp));
  }
  out.qtime = sqlite3_column_int64(stmt, 3);
  tmp = sqlite3_column_text(stmt, 4);
  if (!!tmp) {
    out.stime.emplace(sqlite3_column_int64(stmt, 4));
  }
  tmp = sqlite3_column_text(stmt, 5);
  if (!!tmp) {
    out.etime.emplace(sqlite3_column_int64(stmt, 5));
  }
  tmp = sqlite3_column_text(stmt, 6);
  if (!!tmp) {
    out.status.emplace(sqlite3_column_int(stmt, 6));
  }
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }

  return out;
}

std::vector<job_stat> Status_Manager::get_all_job_stats() {
  int sqlite_ret;
  sqlite3_stmt *stmt;

  if ((sqlite_ret = sqlite3_prepare_v2(conn_, get_all_jobs_stmt.data(), -1,
                                       &stmt, nullptr) != SQLITE_OK)) {
    die_with_err("Unable to prepare get jobid statement", sqlite_ret);
  }
  std::vector<job_stat> out;
  while ((sqlite_ret = sqlite3_step(stmt)) == SQLITE_ROW) {
    job_stat tmp_stat;
    tmp_stat.id = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
    tmp_stat.cmd = std::string{
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1))};
    auto tmp = sqlite3_column_text(stmt, 2);
    if (tmp[0] != '\0') {
      tmp_stat.category.emplace(reinterpret_cast<const char *>(tmp));
    }
    tmp_stat.qtime = sqlite3_column_int64(stmt, 3);
    tmp = sqlite3_column_text(stmt, 4);
    if (!!tmp) {
      tmp_stat.stime.emplace(sqlite3_column_int64(stmt, 4));
    }
    tmp = sqlite3_column_text(stmt, 5);
    if (!!tmp) {
      tmp_stat.etime.emplace(sqlite3_column_int64(stmt, 5));
    }
    tmp = sqlite3_column_text(stmt, 6);
    if (!!tmp) {
      tmp_stat.status.emplace(sqlite3_column_int(stmt, 6));
    }
    out.push_back(tmp_stat);
  }
  if (sqlite_ret != SQLITE_DONE) {
    die_with_err("Error retrieving job statuses", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }
  return out;
}

std::vector<job_stat> Status_Manager::get_failed_job_stats() {
  int sqlite_ret;
  sqlite3_stmt *stmt;

  if ((sqlite_ret = sqlite3_prepare_v2(conn_, get_failed_jobs_stmt.data(), -1,
                                       &stmt, nullptr) != SQLITE_OK)) {
    die_with_err("Unable to prepare get jobid statement", sqlite_ret);
  }
  std::vector<job_stat> out;
  while ((sqlite_ret = sqlite3_step(stmt)) == SQLITE_ROW) {
    job_stat tmp_stat;
    tmp_stat.id = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
    tmp_stat.cmd = std::string{
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1))};
    auto tmp = sqlite3_column_text(stmt, 2);
    if (tmp[0] != '\0') {
      tmp_stat.category.emplace(reinterpret_cast<const char *>(tmp));
    }
    tmp_stat.qtime = sqlite3_column_int64(stmt, 3);
    tmp = sqlite3_column_text(stmt, 4);
    if (!!tmp) {
      tmp_stat.stime.emplace(sqlite3_column_int64(stmt, 4));
    }
    tmp = sqlite3_column_text(stmt, 5);
    if (!!tmp) {
      tmp_stat.etime.emplace(sqlite3_column_int64(stmt, 5));
    }
    tmp = sqlite3_column_text(stmt, 6);
    if (!!tmp) {
      tmp_stat.status.emplace(sqlite3_column_int(stmt, 6));
    }
    out.push_back(tmp_stat);
  }
  if (sqlite_ret != SQLITE_DONE) {
    die_with_err("Error retrieving job statuses", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }
  return out;
}

job_details Status_Manager::get_job_details_by_id(uint32_t id) {
  int sqlite_ret;
  sqlite3_stmt *stmt;

  if ((sqlite_ret =
           sqlite3_prepare_v2(
               conn_, std::format(get_job_details_by_id_stmt, id).c_str(), -1,
               &stmt, nullptr) != SQLITE_OK)) {
    die_with_err("Unable to prepare get jobid statement", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_ROW) {
    die_with_err("Unable get job detail data", sqlite_ret);
  }
  job_details out;
  out.stat.id = static_cast<uint32_t>(sqlite3_column_int(stmt, 0));
  out.stat.cmd =
      std::string{reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1))};
  auto tmp = sqlite3_column_text(stmt, 2);
  if (tmp[0] != '\0') {
    out.stat.category.emplace(reinterpret_cast<const char *>(tmp));
  }
  out.stat.qtime = sqlite3_column_int64(stmt, 3);
  tmp = sqlite3_column_text(stmt, 4);
  if (!!tmp) {
    out.stat.stime.emplace(sqlite3_column_int64(stmt, 4));
  }
  tmp = sqlite3_column_text(stmt, 5);
  if (!!tmp) {
    out.stat.etime.emplace(sqlite3_column_int64(stmt, 5));
  }
  tmp = sqlite3_column_text(stmt, 6);
  if (!!tmp) {
    out.stat.status.emplace(sqlite3_column_int(stmt, 6));
  }
  out.uuid =
      std::string{reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7))};
  out.slots = static_cast<uint32_t>(sqlite3_column_int(stmt, 8));
  out.pid.emplace(sqlite3_column_int(stmt, 9));

  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }
  return out;
}

std::string Status_Manager::get_job_stdout(uint32_t id) {
  int sqlite_ret;
  sqlite3_stmt *stmt;
  if ((sqlite_ret =
           sqlite3_prepare_v2(
               conn_, std::format(get_job_output_stmt, "stdout", id).c_str(),
               -1, &stmt, nullptr) != SQLITE_OK)) {
    die_with_err("Unable to prepare get job output statement", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_ROW) {
    die_with_err("Unable get job stdout data", sqlite_ret);
  }
  auto out =
      std::string{reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))};
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }
  return out;
};
std::string Status_Manager::get_job_stderr(uint32_t id) {
  int sqlite_ret;
  sqlite3_stmt *stmt;
  if ((sqlite_ret =
           sqlite3_prepare_v2(
               conn_, std::format(get_job_output_stmt, "stderr", id).c_str(),
               -1, &stmt, nullptr) != SQLITE_OK)) {
    die_with_err("Unable to prepare get job output statement", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_ROW) {
    die_with_err("Unable get job stdout data", sqlite_ret);
  }
  auto out =
      std::string{reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0))};
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }
  return out;
};

std::string Status_Manager::get_cmd_to_rerun(uint32_t id) {
  int sqlite_ret;
  sqlite3_stmt *stmt;
  if ((sqlite_ret = sqlite3_prepare_v2(
                        conn_, std::format(get_cmd_to_rerun_stmt, id).c_str(),
                        -1, &stmt, nullptr) != SQLITE_OK)) {
    die_with_err("Unable to prepare get job output statement", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_ROW) {
    die_with_err("No job with requested id exists", sqlite_ret);
  }
  std::cout << sqlite3_column_bytes(stmt, 0) << std::endl;
  auto out =
      std::string{reinterpret_cast<const char *>(sqlite3_column_blob(stmt, 0)),
                  static_cast<size_t>(sqlite3_column_bytes(stmt, 0))};
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }
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