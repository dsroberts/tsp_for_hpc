#include "locker.hpp"

#include <array>
#include <ranges>
#include <signal.h>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "functions.hpp"

namespace tsp {

std::array<sighandler_t, NSIG> prev_sigs;

int lockfile_fd = -1;
void handle_signal(int sig) {
  if (lockfile_fd != -1) {
    flock(lockfile_fd, LOCK_UN);
    close(lockfile_fd);
  }
  prev_sigs[sig](sig);
}

Locker::Locker() {
  lockfile_fd = open(lock_file_path_.c_str(), O_RDONLY | O_CREAT, 0600);
  if (lockfile_fd == -1) {
    die_with_err_errno("Unable to open lockfile", lockfile_fd);
  }
}

void Locker::lock() {
  auto flock_out = flock(lockfile_fd, LOCK_EX);
  if (flock_out == -1) {
    die_with_err_errno("Unable to lock lockfile", flock_out);
  }
  // Set up signal handler
  for (auto [i, sig] : std::views::enumerate(prev_sigs)) {
    sig = signal(i, handle_signal);
  }
}

void Locker::unlock() {
  auto flock_out = flock(lockfile_fd, LOCK_UN);
  if (flock_out == -1) {
    die_with_err_errno("Unable to unlock lockfile", flock_out);
  }
  for (auto [i, sig] : std::views::enumerate(prev_sigs)) {
    if (sig == SIG_ERR) {
      continue;
    }
    signal(i, sig);
  }
}

Locker::~Locker() {
  unlock();
  if (lockfile_fd != -1) {
    close(lockfile_fd);
  }
}

} // namespace tsp