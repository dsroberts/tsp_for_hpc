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

Status_Manager::Status_Manager()
    : jobid(gen_jobid()), total_slots_(get_cgroup().size()) {
  auto stat_fn = std::filesystem::temp_directory_path() / db_name;
  int sqlite_ret;
  if ((sqlite_ret = sqlite3_open_v2(stat_fn.c_str(), &conn_,
                                    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                        SQLITE_OPEN_FULLMUTEX,
                                    nullptr)) != SQLITE_OK) {
    throw std::runtime_error("Unable to open database");
  }
  // Wait a long time if we have to
  if ((sqlite_ret = sqlite3_busy_timeout(conn_, 10000)) != SQLITE_OK) {
    throw std::runtime_error(std::string("Unable to set busy timeout :"));
  }
  char *sqlite_err;
  if ((sqlite_ret = sqlite3_exec(conn_, db_initialise.c_str(), nullptr, nullptr,
                                 &sqlite_err)) != SQLITE_OK) {
    throw std::runtime_error(std::string("Error initialising database :") +
                             sqlite_err);
  }
}
Status_Manager::~Status_Manager() { sqlite3_close_v2(conn_); }

void Status_Manager::add_cmd(Run_cmd cmd, std::string category,
                             uint32_t nslots) {
  slots_req_ = nslots;
  sqlite3_stmt *stmt;
  int sqlite_ret;
  if ((sqlite_ret = sqlite3_prepare_v2(
           conn_,
           "INSERT INTO jobs(uuid,command,category,pid,slots) VALUES "
           "(?,?,?,?,?)",
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
  if ((sqlite_ret = sqlite3_bind_text(stmt, 3, category.c_str(), -1,
                                      nullptr)) != SQLITE_OK) {
    die_with_err("Unable bind category", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_int(stmt, 4, getpid())) != SQLITE_OK) {
    die_with_err("Unable bind pid", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_bind_int(
           stmt, 5, static_cast<int32_t>(slots_req_))) != SQLITE_OK) {
    die_with_err("Unable bind nslots", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_DONE) {
    die_with_err("Unable insert data", sqlite_ret);
  }
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }

  char *sqlite_err;
  if ((sqlite_ret = sqlite3_exec(conn_,
                                 std::string("INSERT INTO qtime(jobid) SELECT "
                                             "id FROM jobs WHERE uuid = \"" +
                                             jobid + "\";")
                                     .c_str(),
                                 nullptr, nullptr, &sqlite_err)) != SQLITE_OK) {
    die_with_err("Unable to prepare new jobid statement", sqlite_ret);
  }
}

bool Status_Manager::allowed_to_run() {
  sqlite3_stmt *stmt;
  int sqlite_ret;
  uint32_t slots_used;
  if ((sqlite_ret = sqlite3_prepare_v2(conn_, "SELECT s FROM used_slots;", -1,
                                       &stmt, nullptr)) != SQLITE_OK) {
    die_with_err("Unable to prepare get used slots statement", sqlite_ret);
  }
  while ((sqlite_ret = sqlite3_step(stmt)) != SQLITE_DONE) {
    if (sqlite_ret != SQLITE_ROW) {
      die_with_err("Pid row unable to be returned", sqlite_ret);
    }
    slots_used = static_cast<uint32_t>(sqlite3_column_int(stmt,0));
  }
  if ((sqlite_ret = sqlite3_finalize(stmt)) != SQLITE_OK) {
    die_with_err("Unable finalize statement", sqlite_ret);
  }

  return (total_slots_ - slots_used) >= slots_req_;
}

std::vector<pid_t> Status_Manager::get_running_job_pids() {
  sqlite3_stmt *stmt;
  int sqlite_ret;
  std::vector<pid_t> out;
  if ((sqlite_ret = sqlite3_prepare_v2(conn_, "SELECT pid FROM sibling_pids;",
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
                        std::string("INSERT INTO stime(jobid) SELECT id FROM "
                                    "jobs WHERE uuid = \"" +
                                    jobid + "\";")
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
           std::string("INSERT INTO etime(jobid,exit_status) SELECT id," +
                       std::to_string(exit_stat) +
                       " FROM jobs WHERE uuid = \"" + jobid + "\";")
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