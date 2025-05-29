#include <gflags/gflags.h>

#include "util/output.h"
#include "workloads/benchmark.h"

namespace schtest::workloads {

DEFINE_double(min_time, 0.25, "minimum time to run benchmarks");
DEFINE_double(max_time, 10.0, "maximum time to run benchmarks");
DEFINE_double(confidence, 0.95, "confidence threshold for benchmarks");

double converge(Context &ctx, std::function<double(void)> metric,
                double limit) {
  // `missed` indicates the number of times we have seen a consequence decrease
  // in the metric, while `hit` indicates the number of times we have been over
  // the prescribed threshold. We choose to stop converging if we either cross
  // the limit two times in a row (using the same delay to verify), or we miss
  // three times in a row while growing the delay. We count failure to meet the
  // rolling average as a miss also, but you get three shots at that.
  size_t missed = 0;
  size_t hit = 0;
  auto delay = std::chrono::duration<double>(FLAGS_min_time);
  auto max_delay = std::chrono::duration<double>(FLAGS_max_time);
  double last = 0.0;
  double total = 0.0;
  size_t count = 0;
  while (true) {
    if (missed > 0) {
      delay = std::min(delay * 2, max_delay);
    }
    auto ok = ctx.start();
    if (!ok) {
      EXPECT_TRUE(bool(ok)) << ok.takeError();
      return 0.0; // Failed.
    }
    std::this_thread::sleep_for(delay);
    ctx.stop();
    double next = metric();
    total += next;
    count++;
    double avg = total / count;
    if (next >= limit) {
      missed = 0;
      hit++;
    } else if (next >= last && next >= avg) {
      missed = 0;
      hit = 0;
    } else {
      missed++;
      hit = 0;
    }
    if (hit >= 2 || missed >= 3) {
      return next;
    }
    last = next;
  }
}

void benchmark(Context &ctx,
               std::function<folly::QuantileEstimates(void)> metric) {
  auto last = metric(); // Should be estimates.
  double v = converge(
      ctx,
      [&] {
        auto next = metric();
        double metric = similarity(last, next);
        last = next;
        return metric;
      },
      FLAGS_confidence);
  EXPECT_GE(v, FLAGS_confidence)
      << "benchmark did not converge; confidence is " << v;
  output::log() << last;
}

} // namespace schtest::workloads
