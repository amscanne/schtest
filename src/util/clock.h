#pragma once

#include <chrono>
#include <folly/stats/QuantileEstimator.h>

namespace schtest {

class Timer {
public:
  using clock = std::chrono::steady_clock;
  Timer() : start_(clock::now()) {}

  // resest resets the current timer.
  inline void reset() { start_ = clock::now(); }

  // elapsed returns the time since the timer was reset.
  inline std::chrono::nanoseconds elapsed() { return clock::now() - start_; }

private:
  using time_point = std::chrono::time_point<clock, std::chrono::nanoseconds>;
  time_point start_;
};

}; // namespace schtest
