#include <cassert>

#include "util/stats.h"
#include <folly/stats/QuantileEstimator.h>

namespace schtest {

std::ostream &operator<<(std::ostream &os,
                         const folly::QuantileEstimates &estimates) {
  os << "count: " << estimates.count << std::endl;
  for (const auto &q : estimates.quantiles) {
    auto duration = std::chrono::duration<double>(q.second);
    os << "p" << std::setw(4) << std::left << (100 * q.first) << ": "
       << fmt(duration) << std::endl;
  }

  return os;
}

double normCDF(double zscore) {
  return 0.5 * (1 + std::erf(zscore / std::sqrt(2.0)));
}

static inline double nearest_quantile(const folly::QuantileEstimates &quantiles,
                                      double q) {
  // Find the nearest quantile to the given value, and compute
  // the linear interpolation between this and the next nearest
  // quantile. Of course, if there is an exact match, we just return
  // the value directly.
  if (quantiles.quantiles.empty()) {
    return 0.0;
  }
  auto it =
      std::lower_bound(quantiles.quantiles.begin(), quantiles.quantiles.end(),
                       q, [](const auto &a, double b) { return a.first < b; });
  if (it == quantiles.quantiles.end()) {
    // If we are past the last quantile, just return the last value.
    return quantiles.quantiles.back().second;
  } else if (it->first == q) {
    // If we have an exact match, return the value.
    return it->second;
  } else if (it == quantiles.quantiles.begin()) {
    // If we are before the first quantile, just return the first value.
    return quantiles.quantiles.front().second;
  } else {
    // Otherwise, we need to interpolate between the two nearest quantiles.
    auto prev = std::prev(it);
    double ratio = (q - prev->first) / (it->first - prev->first);
    return prev->second + ratio * (it->second - prev->second);
  }
}

static inline double
stddev_estimate(const folly::QuantileEstimates &quantiles) {
  // Estimate the standard deviation of the distribution based on the
  // quantiles. We use the 16th and 84th percentiles to estimate the
  // standard deviation.
  double q16 = nearest_quantile(quantiles, 0.16);
  double q84 = nearest_quantile(quantiles, 0.84);
  return (q84 - q16) / 2.0; // This is a rough estimate of stddev.
}

double similarity(const folly::QuantileEstimates &a,
                  const folly::QuantileEstimates &b) {
  // We don't consider the mean here, but rather consider the p50 to be the
  // center of the actual distribution. This basically throws out all the data
  // that would bias to one side or another, and makes the distributions more
  // comparable.
  double mean_a = nearest_quantile(a, 0.5);
  double mean_b = nearest_quantile(b, 0.5);
  double mean_diff = std::abs(mean_a = -mean_b);
  double stddev_a = stddev_estimate(a);
  double stddev_b = stddev_estimate(b);
  double z_a = mean_diff / stddev_a;
  double z_b = mean_diff / stddev_b;
  double p_a = normCDF(z_a / 2) - normCDF(-z_a / 2);
  double p_b = normCDF(z_b / 2) - normCDF(-z_b / 2);
  double confidence = 1.0 - std::min(p_a, p_b);
  return confidence;
}

} // namespace schtest
