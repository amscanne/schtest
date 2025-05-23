#include <cassert>

#include "util/stats.h"
#include <folly/stats/QuantileEstimator.h>

namespace schtest {

std::ostream &operator<<(std::ostream &os,
                         folly::QuantileEstimates &estimates) {
  os << "count: " << estimates.count << std::endl;
  for (const auto &q : estimates.quantiles) {
    auto orig = std::chrono::duration<double>(q.second);
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(orig);
    os << "p" << std::setw(4) << std::left << (100 * q.first) << ": "
       << ns.count() << std::endl;
  }

  return os;
}

static double zscore(double p) {
  assert(p > 0.5);
  // Abramowitz and Stegun formula 26.2.23.
  p = std::sqrt(-2.0 * log(1 - p));
  double c[] = {2.515517, 0.802853, 0.010328};
  double d[] = {1.432788, 0.189269, 0.001308};
  return p - ((c[2] * p + c[1]) * p + c[0]) /
                 (((d[2] * p + d[1]) * p + d[0]) * p + 1.0);
}

bool similar(folly::SimpleQuantileEstimator<Timer::clock> &ae,
             folly::SimpleQuantileEstimator<Timer::clock> &be,
             double confidence) {
  ae.flush();
  be.flush();
  const std::array<double, 3> kQuantiles{{.16, 0.50, 0.84}};
  auto a = ae.estimateQuantiles(kQuantiles);
  auto b = be.estimateQuantiles(kQuantiles);

  // We don't consider the mean here, but rather consider the p50 to be the
  // center of the actual distribution. This basically throws out all the data
  // that would bias to one side or another, and makes the distributions more
  // comparable.
  double meanA = a.quantiles[1].second;
  double meanB = b.quantiles[1].second;
  double stdDevA = (a.quantiles[2].second - a.quantiles[0].second) / 2.0;
  double stdDevB = (b.quantiles[2].second - b.quantiles[0].second) / 2.0;

  // Compute our standard error for each of the distributions. Not that this
  // is not *really* the standard error, since we take the `log` of the count
  // rather than simply the square root. This is to ensure that we don't
  // penalize larger counts, since we are not actually a normal distribution.
  double stdErrA = stdDevA / std::log(a.count);
  double stdErrB = stdDevB / std::log(b.count);
  double thresh = zscore(confidence);

  if ((meanA + (thresh * stdErrA)) >= meanB &&
      (meanA - (thresh * stdErrA)) <= meanB &&
      (meanB + (thresh * stdErrB)) >= meanA &&
      (meanB - (thresh * stdErrB)) <= meanA) {
    return true;
  }
  return false;
}

} // namespace schtest
