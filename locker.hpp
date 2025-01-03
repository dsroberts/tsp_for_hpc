#pragma once

#include <string>

#include "functions.hpp"

namespace tsp {
class Locker {
public:
  Locker();
  ~Locker();
  void lock();
  void unlock();

private:
  const std::string lock_file_path_{get_tmp() / ".affinity_lock_file.lock"};
  int lockfile_fd_;
};
} // namespace tsp