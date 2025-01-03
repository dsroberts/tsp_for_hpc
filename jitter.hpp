#pragma once
#include <chrono>
#include <cstdint>
#include <random>

namespace tsp {
constexpr std::chrono::milliseconds jitter_ms{250};
class Jitter {
public:
  Jitter(std::chrono::milliseconds limit);
  std::chrono::milliseconds get();

private:
  std::mt19937 rng_;
  std::uniform_int_distribution<int64_t> dist_;
};
} // namespace tsp