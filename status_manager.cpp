#include "status_manager.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <sqlite3.h>
#include <string>

#include "functions.hpp"
#include "run_cmd.hpp"

namespace tsp {

Status_Manager::Status_Manager()
    : jobid(gen_jobid()), started(false), total_slots(get_cgroup().size()) {
  auto stat_fn = std::filesystem::temp_directory_path() / db_name;
  if ((sqlite_ret = sqlite3_open_v2(stat_fn.c_str(), &conn,
                                    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                        SQLITE_OPEN_FULLMUTEX,
                                    nullptr)) != SQLITE_OK) {
    throw std::runtime_error("Unable to open database");
  }
  // Wait a long time if we have to
  if ((sqlite_ret = sqlite3_busy_timeout(conn, 10000)) != SQLITE_OK) {
    throw std::runtime_error(std::string("Unable to set busy timeout :"));
  }
  if ((sqlite_ret = sqlite3_exec(conn, db_initialise.c_str(), nullptr, nullptr,
                                 &sqlite_err)) != SQLITE_OK) {
    throw std::runtime_error(std::string("Error initialising database :") +
                             sqlite_err);
  }
}
Status_Manager::~Status_Manager() { sqlite3_close_v2(conn); }

void Status_Manager::add_cmd(Run_cmd cmd, std::string category,
                             uint32_t nslots) {
  slots_req = nslots;
  if ((sqlite_ret = sqlite3_exec(
           conn,
           std::string(
               "INSERT INTO jobs(uuid,command,category,slots) VALUES (\"" +
               jobid + "\",\"" + cmd.print() + "\",\"" + category + "\"," +
               std::to_string(slots_req) + ");")
               .c_str(),
           nullptr, nullptr, &sqlite_err)) != SQLITE_OK) {
    throw std::runtime_error(
        std::string("Unable to insert command into database: ") + sqlite_err);
  }
  if ((sqlite_ret = sqlite3_exec(
           conn,
           std::string(
               "INSERT INTO qtime(jobid) SELECT id FROM jobs WHERE uuid = \"" +
               jobid + "\";")
               .c_str(),
           nullptr, nullptr, &sqlite_err)) != SQLITE_OK) {
    throw std::runtime_error(
        std::string("Unable to insert qtime into database: ") + sqlite_err);
  }
}

int Status_Manager::slots_callback(void *slots_avail_void, int ncols,
                                   char **out, char **cols) {
  uint32_t *slots_used = reinterpret_cast<uint32_t *>(slots_avail_void);
  try {
    *slots_used = std::stoi(std::string(out[0]));
    return 0;
  } catch (std::exception_ptr e) {
    return 1;
  }
}

bool Status_Manager::allowed_to_run() {
  if ((sqlite_ret = sqlite3_exec(
           conn, "SELECT * FROM used_slots", Status_Manager::slots_callback,
           reinterpret_cast<void *>(&slots_used), &sqlite_err)) != SQLITE_OK) {
    throw std::runtime_error(
        std::string("Unable to get used slots from database: ") + sqlite_err);
  }
  return (total_slots - slots_used) >= slots_req;
}

void Status_Manager::job_start() {
  if ((sqlite_ret = sqlite3_exec(
           conn,
           std::string(
               "INSERT INTO stime(jobid) SELECT id FROM jobs WHERE uuid = \"" +
               jobid + "\";")
               .c_str(),
           nullptr, nullptr, &sqlite_err)) != SQLITE_OK) {
    throw std::runtime_error(
        std::string("Unable to insert stime into database: ") + sqlite_err);
  }
  started = true;
}

void Status_Manager::job_end(int exit_stat) {
  if ((sqlite_ret = sqlite3_exec(
           conn,
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
  if ((sqlite_ret = sqlite3_exec(
           conn,
           std::string(
               "INSERT INTO job_output(jobid,stdout,stderr) SELECT id, \"" +
               in.first + "\" , \"" + in.second +
               "\" FROM jobs WHERE uuid = \"" + jobid + "\";")
               .c_str(),
           nullptr, nullptr, &sqlite_err)) != SQLITE_OK) {
    throw std::runtime_error(
        std::string("Unable to insert stdout/stderr into database: ") +
        sqlite_err);
  }
}

std::string Status_Manager::time_writer(
    std::chrono::time_point<std::chrono::system_clock> in) {
  const std::time_t t_c = std::chrono::system_clock::to_time_t(in);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&t_c), "%c");
  return ss.str();
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