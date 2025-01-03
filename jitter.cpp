#include "jitter.hpp"

#include <chrono>
#include <random>

namespace tsp {
Jitter::Jitter(std::chrono::milliseconds limit)
    : rng_(std::mt19937(std::random_device{}())),
      dist_(std::uniform_int_distribution<int64_t>(-abs(limit).count(),
                                                  abs(limit).count())) {}
std::chrono::milliseconds Jitter::get() {
  return std::chrono::milliseconds(dist_(rng_));
}
} // namespace tsp