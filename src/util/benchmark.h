#pragma once

#include <memory>

#include "util/stats.h"

namespace schtest::benchmark {

// `run` captures a distribution by running a function repeatedly, and testing
// to see whether the distributions converge. Once we run with three different
// distributions that all represent the same thing, then we are finished.
template <typename T>
std::unique_ptr<Distribution<T>>
run(std::ostream &os, size_t start_iters,
    std::function<void(size_t iters, Distribution<T> &d)> fn,
    double confidence = 0.95) {
  size_t iters = start_iters;
  size_t hit = 0;
  size_t missed = 0;
  auto last = std::make_unique<Distribution<T>>();
  while (true) {
    auto next = std::make_unique<Distribution<T>>();
    fn(iters, *next);
    if (similar(*last, *next, confidence)) {
      os << "☑";
      os.flush();
      hit++;
      missed = 0;
    } else {
      os << "☐";
      os.flush();
      missed++;
      hit = 0;
    }
    if (hit >= 2) {
      return std::move(next);
    }
    if (missed >= 2) {
      iters = (iters << 2) / 3;
      missed = 0;
    }
    last = std::move(next);
  }
  os << std::endl;
}

} // namespace schtest::benchmark
