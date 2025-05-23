#include "util/clock.h"

namespace schtest {

std::ostream& operator<<(std::ostream &os, TimeDistribution& d)
{
  const std::array<double, 5> kQuantiles{{.001, .01, .5, .99, .999}};
  d.estimator_.flush();
  auto estimates = d.estimator_.estimateQuantiles(kQuantiles);

  os << "count: " << estimates.count << std::endl;
  for (const auto &q : estimates.quantiles) {
    auto orig = std::chrono::duration<double>(q.second);
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(orig);
    os << "p" << std::setw(4) << std::left << (100*q.first) << ": " << ns.count() << std::endl;
  }

  return os;
}


} // namespace schtest
