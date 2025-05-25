#include <cassert>

#include "util/clock.h"
#include "util/stats.h"

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

static double width(const folly::QuantileEstimates &a, size_t index) {
  double before = 0.0;
  if (index == 0) {
    before = a.quantiles[0].first;
  } else {
    before = (a.quantiles[index].first - a.quantiles[index - 1].first) / 2.0;
  }
  double after = 0.0;
  if (index == a.quantiles.size() - 1) {
    after = 1.0 - a.quantiles[index].first;
  } else {
    after = (a.quantiles[index + 1].first - a.quantiles[index].first) / 2.0;
  }
  return before + after;
}

static double distance(const folly::QuantileEstimates &a,
                       const folly::QuantileEstimates &b) {
  // This function computes the Kolmogorov–Smirnov distance between two
  // different distributions. Since we have the quantiles, we need to do
  // it a bit sideways, but it's the same idea; the difference between
  // thw two CDF functions (which fits in the [0.0, 1.0] range).
  assert(a.quantiles.size() == b.quantiles.size());
  double distance = 0.0;
  double total_min =
      std::min(a.quantiles.front().second, b.quantiles.front().second);
  double total_max =
      std::max(a.quantiles.back().second, b.quantiles.back().second);
  double total = total_max - total_min;
  for (size_t i = 0; i < a.quantiles.size(); i++) {
    assert(a.quantiles[i].first == b.quantiles[i].first);
    double delta = std::abs(a.quantiles[i].second - b.quantiles[i].second);
    distance += width(a, i) * delta;
  }
  return distance / (total_max - total_min);
}

double similarity(const folly::QuantileEstimates &a,
                  const folly::QuantileEstimates &b) {
  return 1.0 - distance(a, b);
}

} // namespace schtest
