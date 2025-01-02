#include "jitter.hpp"

#include <chrono>
#include <random>

namespace tsp {
Jitter::Jitter(std::chrono::milliseconds limit)
    : rng(std::mt19937(std::random_device{}())),
      dist(std::uniform_int_distribution<int64_t>(-abs(limit).count(),
                                                  abs(limit).count())) {}
std::chrono::milliseconds Jitter::get() {
  return std::chrono::milliseconds(dist(rng));
}
} // namespace tsp