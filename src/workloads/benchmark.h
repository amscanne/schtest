#pragma once

#include <functional>

#include "util/output.h"
#include "util/stats.h"
#include "workloads/context.h"

namespace schtest::workloads {

constexpr double kDefaultConfidence = 0.95;

// `converge` is a helper function that takes a function that produces
// a metric in the range [0.0, 1.0]. The function starts with 100ms, and
// increases the time until it finds the metric is no longer converging.
//
// This function does not record any error on failure to converge, but
// reports the final metric value.
double converge(Context &ctx, std::function<double(void)> metric,
                double confidence = kDefaultConfidence);

// `benchmark` captures a distribution by running a function repeatedly, and
// testing to see whether the distributions converge. We use the distance of
// the quantile estimates as our metric function above.
//
// This function will cause the test to fail if the distribution does not
// converge as expected.
template <typename Fn>
void benchmark(Context &ctx, Fn fn, double confidence = kDefaultConfidence) {
  auto last = fn(); // Should be estimates.
  double v = converge(ctx, [&] {
    auto next = fn();
    double metric = similarity(last, next);
    last = next;
    return metric;
  });
  EXPECT_GE(v, confidence);
  output::log() << last;
}

} // namespace schtest::workloads
