#include "memprof_manager.hpp"

#include <cstdint>
#include <sqlite3.h>
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
  for (const auto &proc : data) {
    ssm.step_put(time, proc.jobid, proc.vmem, proc.rss, proc.pss, proc.shared,
                 proc.swap, proc.swap_pss);
  }
}

std::vector<std::pair<uint32_t, pid_t>>
Memprof_Manager::get_running_job_ids_and_pids() {
  if (!conn_) {
    die_with_err("Database connection has failed", -1);
  }
  std::vector<std::pair<uint32_t, pid_t>> out;
  auto ssm = Sqlite_statement_manager(conn_, get_ids_and_pids);
  uint32_t jobid;
  pid_t pid;
  while (ssm.step_get(jobid, pid)) {
    // while (ssm.step() != SQLITE_DONE) {
    out.emplace_back(jobid, pid);
  }
  return out;
}

} // namespace tsp
