#pragma once

#include <chrono>
#include <cmath>
#include <folly/stats/QuantileEstimator.h>
#include <folly/stats/QuantileHistogram.h>
#include <gtest/gtest_prod.h>
#include <iostream>

namespace folly {

std::ostream &operator<<(std::ostream &os, const QuantileEstimates &estimates);

} // namespace folly

namespace schtest {

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

// Defined in stats.cpp.
double similarity(const folly::QuantileEstimates &a,
                  const folly::QuantileEstimates &b);

// A sampler that can be used for a single element.
//
// This is a fixed-size structure and therefore it can be embedded safely
// into shared memory types.
template <typename T, size_t S>
class Sampler {
public:
  void sample(T v) {
    // Record in a ring buffer if we exhaust the limit.
    auto index = index_.fetch_add(1);
    samples_[index % S] = v;
  }

  void flush(Distribution<T> &dist) {
    // Read all valid samples.
    auto index = index_.load();
    for (size_t i = 0; i < S && i < index; i++) {
      dist.sample(samples_[i]);
    }
    index_.store(0);
  }

private:
  std::array<T, S> samples_ = {};
  std::atomic<uint32_t> index_ = 0;
};

// Aggregate statistics for a given test.
class Stats {
public:
  Distribution<std::chrono::nanoseconds> wake_latency;
};

}; // namespace schtest
