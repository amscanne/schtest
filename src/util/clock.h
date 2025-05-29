#pragma once

#include <chrono>
#include <folly/stats/QuantileEstimator.h>
#include <iostream>

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
  using time_point = std::chrono::time_point<clock, std::chrono::nanoseconds>;

  Timer() : start_(clock::now()){};

  // reset resets the current timer.
  void reset() { start_.store(clock::now()); }

  // returns the time passed since the last reset.
  std::chrono::nanoseconds elapsed() const {
    auto now = clock::now();
    auto start = start_.load();
    if (now < start) {
      return std::chrono::nanoseconds(0);
    }
    return now - start;
  }

private:
  std::atomic<time_point> start_;
};

template <size_t S>
class RobustTimer {
public:
  using cookie_t = uint64_t;

  void reset() {
    auto orig_index = index_.fetch_add(1);
    timers_[orig_index % S].reset();
  }

  // Returns a unique cookie that can be used to disambiguate the timer when
  // it is reset multiple times between calls to `reset` and `elapsed`.
  cookie_t cookie() { return index_.load(); }

  // Returns the time passed since the last reset prior to the given cookie. If
  // this is not available, then `std::nullopt` is returned instead.
  std::optional<std::chrono::nanoseconds> elapsed(cookie_t cookie) const {
    auto v = timers_[cookie % S].elapsed();
    if (index_.load() >= cookie + S) {
      // This is no longer valid.
      return std::nullopt;
    }
    return v;
  }

private:
  std::array<Timer, S> timers_ = {};
  std::atomic<cookie_t> index_ = 0;
};

}; // namespace schtest
