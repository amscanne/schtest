#pragma once

#include <chrono>
#include <folly/stats/QuantileEstimator.h>
#include <iostream>

#include "util/clock.h"

namespace schtest {

template <typename T, size_t B>
class Histogram {
public:
  Histogram(T min, T max)
      : min_(min), max_(max), width_((max - min) / static_cast<T>(B)),
        samples_(0) {
    // Ensure that the width is sensible.
    if constexpr (std::is_same_v<T, std::chrono::nanoseconds>) {
      if (width_ == std::chrono::nanoseconds::zero()) {
        buckets_.resize(1);
      } else {
        buckets_.resize(B + 1);
      }
    } else {
      if (width_ == 0) {
        buckets_.resize(1);
      } else {
        buckets_.resize(B + 1);
      }
    }
  }
  void add(T data) {
    samples_++;
    if (buckets_.size() == 1) {
      buckets_[0]++;
    } else {
      buckets_[static_cast<size_t>((data - min_) / width_)]++;
    }
  }

private:
  T min_;
  T max_;
  T width_;
  size_t samples_;
  std::vector<size_t> buckets_;

  // For printing below.
  template <typename U, size_t C>
  friend std::ostream &operator<<(std::ostream &os, const Histogram<U, C> &h);
};

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

  template <size_t B>
  Histogram<T, B> histogram() {
    constexpr double minq = 0.001;
    constexpr double maxq = 0.999;
    estimator_.flush();
    auto digest = estimator_.getDigest();
    auto min = restore(digest.estimateQuantile(minq));
    auto max = restore(digest.estimateQuantile(maxq));
    Histogram<T, B> h(min, max);
    for (double p = minq; p <= maxq; p += 0.001) {
      h.add(restore(digest.estimateQuantile(p)));
    }
    return h;
  }

private:
  folly::SimpleQuantileEstimator<Timer::clock> estimator_;

  // Restore the original data type from an estimate.
  T restore(double v) {
    if constexpr (std::is_same_v<T, std::chrono::nanoseconds>) {
      return std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::duration<double>(v));
    } else {
      return static_cast<T>(v);
    }
  }

  // For printing below.
  template <typename U>
  friend std::ostream &operator<<(std::ostream &os, Distribution<U> &);

  // For comparison of distributions.
  template <typename U>
  friend double similarity(Distribution<U> &a, Distribution<U> &b);
};

// Convenience definitions.
using LatencyDistribution = Distribution<std::chrono::nanoseconds>;

// Pretty print a histogram.
template <typename T, size_t B>
std::ostream &operator<<(std::ostream &os, const Histogram<T, B> &h) {
  constexpr size_t bar_width = 30;
  auto value = h.min_;
  for (const auto &b : h.buckets_) {
    os << "[";
    size_t chars = static_cast<size_t>(std::ceil(
        static_cast<double>(bar_width * b) / static_cast<double>(h.samples_)));
    size_t done = 0;
    for (; done < chars; done++) {
      os << "#";
    }
    for (; done < bar_width; done++) {
      os << " ";
    }
    os << "] ";
    os << value;
    os << std::endl;
    value += h.width_;
  }
  return os;
}

// Defined in stats.cpp.
std::ostream &operator<<(std::ostream &os, folly::QuantileEstimates &estimates);

template <typename T>
std::ostream &operator<<(std::ostream &os, Distribution<T> &d) {
  constexpr std::array<double, 7> kQuantiles{
      {.001, .01, .1, .5, .9, .99, .999}};
  d.estimator_.flush();
  auto estimates = d.estimator_.estimateQuantiles(kQuantiles);
  os << estimates;
  return os;
}

// Defined in stats.cpp.
double similarity(folly::SimpleQuantileEstimator<Timer::clock> &a,
                  folly::SimpleQuantileEstimator<Timer::clock> &b);

// Determine whether there are statistically significant differences between
// the two distributions, based on the given confidence.
template <typename T>
double similarity(Distribution<T> &a, Distribution<T> &b) {
  return similarity(a.estimator_, b.estimator_);
}

}; // namespace schtest
