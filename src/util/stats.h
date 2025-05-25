#pragma once

#include <chrono>
#include <cmath>
#include <folly/stats/QuantileEstimator.h>
#include <folly/stats/QuantileHistogram.h>
#include <gtest/gtest_prod.h>
#include <iostream>

namespace schtest {

// Defined in stats.cpp.
std::ostream &operator<<(std::ostream &os,
                         const folly::QuantileEstimates &estimates);

template <typename T>
class Distribution {
public:
  // Add a single sample to the distribution.
  void sample(T data) {
    double v;
    if constexpr (std::is_same_v<T, std::chrono::nanoseconds>) {
      v = std::chrono::duration<double>(data).count();
    } else {
      v = static_cast<double>(data);
    }
    estimator_.addValue(v);
  }

  // Return the current estimates of the distribution.
  folly::QuantileEstimates estimates() {
    folly::QuantileEstimates estimates = {};
    estimates.count = estimator_.count();
    auto quantiles = estimator_.quantiles();
    estimates.quantiles.reserve(quantiles.size());
    for (const auto &q : quantiles) {
      auto v = estimator_.estimateQuantile(q);
      estimates.quantiles.emplace_back(q, v);
    }
    return estimates;
  }

private:
  folly::CPUShardedQuantileHistogram<> estimator_;

  // Restore the original data type from an estimate.
  T restore(double v) {
    if constexpr (std::is_same_v<T, std::chrono::nanoseconds>) {
      return std::chrono::nanoseconds(static_cast<long long>(v));
    } else {
      return static_cast<T>(v);
    }
  }
};

// Convenience definitions.
using LatencyDistribution = Distribution<std::chrono::nanoseconds>;

// Defined in stats.cpp.
double similarity(const folly::QuantileEstimates &a,
                  const folly::QuantileEstimates &b);

// Determine whether there are statistically significant differences between
// the two distributions, based on the given confidence.
template <typename T>
double similarity(Distribution<T> &a, Distribution<T> &b) {
  return similarity(a.estimates(), b.estimates());
}

}; // namespace schtest
