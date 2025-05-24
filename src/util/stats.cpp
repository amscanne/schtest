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

double normCDF(double zscore) {
  return 0.5 * (1 + std::erf(zscore / std::sqrt(2.0)));
}

double similarity(folly::SimpleQuantileEstimator<Timer::clock> &ae,
                  folly::SimpleQuantileEstimator<Timer::clock> &be) {
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
  double ZA = std::abs(meanA - meanB) / stdDevA;
  double ZB = std::abs(meanA - meanB) / stdDevB;
  double PA = normCDF(ZA / 2) - normCDF(-ZA / 2);
  double PB = normCDF(ZB / 2) - normCDF(-ZB / 2);
  double confidence = 1.0 - std::min(PA, PB);
  return confidence;
}

} // namespace schtest