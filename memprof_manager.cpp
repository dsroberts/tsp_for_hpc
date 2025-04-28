#include "memprof_manager.hpp"

#include <cstdint>
#include <sqlite3.h>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "sqlite_statement_manager.hpp"

namespace tsp {

Memprof_Manager::Memprof_Manager() : Status_Manager() {
  int sqlite_ret;
  char *sqlite_err;
  if ((sqlite_ret = sqlite3_exec(conn_, memprof_init.data(), nullptr, nullptr,
                                 &sqlite_err)) != SQLITE_OK) {
    exit_with_sqlite_err(sqlite_err, db_initialise, sqlite_ret, conn_);
  }
}

void Memprof_Manager::memprof_update(int64_t time, std::vector<mem_data> data) {
  if (!rw_) {
    die_with_err("Attempted to write to database in read-only mode!", -1);
  }
  auto ssm = Sqlite_statement_manager(conn_, insert_memprof_data);
  int sqlite_ret;
  for (const auto &proc : data) {
    if ((sqlite_ret = sqlite3_bind_int64(ssm.stmt, 1, time)) != SQLITE_OK) {
      die_with_err("Unable bind timestamp", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_int(ssm.stmt, 2, proc.jobid)) != SQLITE_OK) {
      die_with_err("Unable bind jobid", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_int64(ssm.stmt, 3, proc.vmem)) !=
        SQLITE_OK) {
      die_with_err("Unable bind jobid", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_int64(ssm.stmt, 4, proc.rss)) != SQLITE_OK) {
      die_with_err("Unable bind jobid", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_int64(ssm.stmt, 5, proc.pss)) != SQLITE_OK) {
      die_with_err("Unable bind jobid", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_int64(ssm.stmt, 6, proc.shared)) !=
        SQLITE_OK) {
      die_with_err("Unable bind jobid", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_int64(ssm.stmt, 7, proc.swap)) !=
        SQLITE_OK) {
      die_with_err("Unable bind jobid", sqlite_ret);
    }
    if ((sqlite_ret = sqlite3_bind_int64(ssm.stmt, 8, proc.swap_pss)) !=
        SQLITE_OK) {
      die_with_err("Unable bind jobid", sqlite_ret);
    }
    ssm.step_and_reset();
  }
}

std::vector<std::pair<uint32_t, pid_t>>
Memprof_Manager::get_running_job_ids_and_pids() {
  if (!conn_) {
    die_with_err("Database connection has failed", -1);
  }
  std::vector<std::pair<uint32_t, pid_t>> out;
  auto ssm = Sqlite_statement_manager(conn_, get_ids_and_pids);
  while (ssm.step() != SQLITE_DONE) {
    out.emplace_back(sqlite3_column_int(ssm.stmt, 0),
                     sqlite3_column_int(ssm.stmt, 1));
  }
  return out;
}

} // namespace tsp
