#pragma once

#include <memory>

#include "util/stats.h"

namespace schtest::benchmark {

// `converge` is a helper function that takes a function that produces
// a metric in the range [0.0, 1.0]. This function takes a single argument,
// `longer` which indicates that it may want to try harder. This indicates
// that the value is not converging currently, and we may give up soon.
//
// This function will be called repeatedly until it either converges, or
// ceases to converge. The final result will be spit back out once there
// are no more changes to the metric.
template <typename Fn>
double converge(double limit, Fn fn) {
  constexpr double decay = 0.3;
  double last = 0.0;
  double next = 0.0;
  size_t missed = 0;
  while (true) {
    next = fn(missed > 1);
    if (next > last) {
      missed = 0;
    } else {
      missed++;
    }
    last = decay * last + (1.0 - decay) * next;
    if (last >= limit) {
      return last;
    }
    if (missed > 3) {
      // We gave two separate changes to grow, but did not have any subsequent
      // convergence. This is probably as good as it's gonna get, so give up.
      return last;
    }
  }
}

// `run` captures a distribution by running a function repeatedly, and testing
// to see whether the distributions converge. Once we run with three different
// distributions that all represent the same thing, then we are finished.
template <typename T>
std::unique_ptr<Distribution<T>>
run(std::ostream &os, size_t start_iters, double confidence,
    std::function<void(size_t iters, Distribution<T> &d)> fn) {
  auto last = std::make_unique<Distribution<T>>();
  auto v = converge(confidence, [&](bool longer) {
    if (longer) {
      start_iters = (start_iters << 2) / 3;
    }
    auto next = std::make_unique<Distribution<T>>();
    fn(start_iters, *next);
    double rv = similarity(*last, *next);
    last = std::move(next);
    return rv;
  });
  EXPECT_GE(v, confidence);
  return std::move(last);
}

} // namespace schtest::benchmark
