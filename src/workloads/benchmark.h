#pragma once

#include <functional>

#include "workloads/context.h"

namespace schtest::workloads {

// `converge` is a helper function that takes a function that produces
// a metric in the range [0.0, 1.0]. The function starts with 100ms, and
// increases the time until it finds the metric is no longer converging.
//
// This function does not record any error on failure to converge, but
// reports the final metric value.
double converge(Context &ctx, std::function<double(void)> metric, double limit);

// `benchmark` captures a distribution by running a function repeatedly, and
// testing to see whether the distributions converge. We use the distance of
// the quantile estimates as our metric function above.
//
// This function will cause the test to fail if the distribution does not
// converge as expected.
void benchmark(Context &ctx,
               std::function<folly::QuantileEstimates(void)> metric);

} // namespace schtest::workloads
