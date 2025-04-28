#pragma once

#include <string>
#include <utility>
#include <vector>

#include "proc_tools.hpp"
#include "status_manager.hpp"

namespace tsp {

constexpr std::string_view memprof_init(
    // Create memprof table
    "CREATE TABLE IF NOT EXISTS memprof (jobid INTEGER NOT NULL, time INTEGER, "
    "vmem INTEGER, rss INTEGER, pss INTEGER, shared INTEGER, swap INTEGER, "
    "swap_pss INTEGER, FOREIGN KEY(jobid) REFERENCES jobs(id) ON DELETE "
    "CASCADE);\0");

constexpr std::string_view insert_memprof_data(
    "INSERT INTO memprof(time,jobid,vmem,rss,pss,shared,swap,swap_pss) "
    "VALUES (?,?,?,?,?,?,?,?);\0");

constexpr std::string_view
    get_ids_and_pids("SELECT id,pid FROM sibling_pids;\0");

class Memprof_Manager : public Status_Manager {
public:
  Memprof_Manager();
  void memprof_update(int64_t time, std::vector<mem_data> data);
  std::vector<std::pair<uint32_t, pid_t>> get_running_job_ids_and_pids();
};

} // namespace tsp