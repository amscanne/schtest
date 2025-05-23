#pragma once

#include <chrono>
#include <folly/stats/QuantileEstimator.h>
#include <iostream>

namespace schtest {

class Timer {
public:
  Timer() : start_(clock::now()) {}

  // resest resets the current timer.
  void reset() {
    start_ = clock::now();
  }

  // elapsed returns the time since the timer was reset.
  std::chrono::nanoseconds elapsed() {
    return clock::now() - start_;
  }

private:
  using clock = std::chrono::steady_clock;
  using nanoseconds = std::chrono::nanoseconds;
  using time_point = std::chrono::time_point<clock, nanoseconds>;
  time_point start_;

  // For types above.
  friend class TimeDistribution;
};

class TimeDistribution {
public:
  // Add a single sample to the distribution.
  void sample(std::chrono::nanoseconds ns) {
    double v = std::chrono::duration<double>(ns).count();
    estimator_.addValue(v);
  }

private:
  folly::SimpleQuantileEstimator<Timer::clock> estimator_;

  // For printing below.
  friend std::ostream& operator<<(std::ostream &os, TimeDistribution&);
};

std::ostream& operator<<(std::ostream &os, TimeDistribution&);

}; // namespace schtest
