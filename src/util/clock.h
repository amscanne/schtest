#pragma once

#include <chrono>
#include <folly/stats/QuantileEstimator.h>

namespace schtest {

std::string fmt(const std::chrono::nanoseconds &d);

template <typename T>
std::string fmt(const std::chrono::duration<T> &d) {
  auto ns = duration_cast<std::chrono::nanoseconds>(d);
  return fmt(ns);
}

class Timer {
public:
  using clock = std::chrono::steady_clock;
  Timer() : start_(clock::now()) {}

  // resest resets the current timer.
  inline void reset() { start_ = clock::now(); }

  // elapsed returns the time since the timer was reset.
  inline std::chrono::nanoseconds elapsed() {
    auto now = clock::now();
    if (now < start_) {
      // If the time is going to go backwards, we just return 0. This
      // can happen if there is a clock change or skew between CPUs,
      // (even though it shouldn't, we at least don't explode).
      return std::chrono::nanoseconds(0);
    }
    return now - start_;
  }

private:
  using time_point = std::chrono::time_point<clock, std::chrono::nanoseconds>;
  time_point start_;
};

}; // namespace schtest
