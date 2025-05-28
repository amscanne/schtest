#include "workloads/benchmark.h"

namespace schtest::workloads {

double converge(Context &ctx, std::function<double(void)> metric,
                double limit) {
  auto delay = std::chrono::milliseconds(100);
  constexpr double decay = 0.3;
  double last = 0.0;
  double next = 0.0;
  size_t missed = 0;
  while (true) {
    if (missed > 1) {
      delay = (delay * 3) / 2;
    }
    auto ok = ctx.start();
    if (!ok) {
      EXPECT_TRUE(bool(ok)) << ok.takeError();
      return 0.0; // Failed.
    }
    std::this_thread::sleep_for(delay);
    ctx.stop();
    next = metric();
    output::log() << delay << " -> " << next;
    if (next > last) {
      missed = 0;
    } else {
      missed++;
    }
    last = decay * last + (1.0 - decay) * next;
    if (last >= limit) {
      return last;
    }
    if (missed > 3) {
      // We gave two separate changes to grow, but did not have any subsequent
      // convergence. This is probably as good as it's gonna get, so give up.
      return last;
    }
  }
}

} // namespace schtest::workloads
