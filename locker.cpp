#include "locker.hpp"

#include <fcntl.h>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "functions.hpp"

namespace tsp {

Locker::Locker()
    : lockfile_fd_{open(lock_file_path_.c_str(), O_RDONLY | O_CREAT, 0600)} {
  if (lockfile_fd_ == -1) {
    die_with_err_errno("Unable to open lockfile", lockfile_fd_);
  }
}

void Locker::lock() {
  auto flock_out = flock(lockfile_fd_, LOCK_EX);
  if (flock_out == -1) {
    die_with_err_errno("Unable to lock lockfile", flock_out);
  }
}

void Locker::unlock() {
  auto flock_out = flock(lockfile_fd_, LOCK_UN);
  if (flock_out == -1) {
    die_with_err_errno("Unable to unlock lockfile", flock_out);
  }
}

Locker::~Locker() {
  unlock();
  if (lockfile_fd_ != -1) {
    close(lockfile_fd_);
  }
}

} // namespace tsp