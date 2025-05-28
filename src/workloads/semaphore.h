#pragma once

#include <cstdint>

#include "util/clock.h"
#include "util/stats.h"

namespace schtest::workloads {

class Context;

// Semaphore is a basic counting semaphore, that will block and unblock
// thread execution that hits it. Calling `consume` will block the current
// thread until the semaphore has sufficient count available, which is done by
// calling `produce`. If `produce` is called ahead of `consume`, then the thread
// will be unblocked immediately and not wait.
//
// To avoid any additional contention, all operations on the semaphore
// are lock-free. The type is not copyable or movable, because the we
// use the address of the `count_` to synchronize threads.
//
// This is distict from the `SocketPair`, as it allows for a "thundering-herd"
// wake-up effect, where multiple threads can be woken up at the same time.
class Semaphore : public Sampler<std::chrono::nanoseconds, 64 * 1024> {
public:
  Semaphore(Context &ctx, uint32_t max = 0x7fffffff) : max_(max){};
  ~Semaphore() = default;

  Semaphore(const Semaphore &) = delete;
  Semaphore(Semaphore &&) = delete;
  Semaphore &operator=(const Semaphore &) = delete;
  Semaphore &operator=(Semaphore &&) = delete;

  void reset() { count_.store(0); }
  void consume(uint32_t v, uint32_t wake = 1);
  void produce(uint32_t v, uint32_t wake = 1);
  uint32_t max() const { return max_; }

private:
  std::atomic<uint32_t> count_ = 0;
  const uint32_t max_;

  // The `wake_` field tracks when the semaphore was notifier (either for
  // producer or consumer), and when the thread that was notifier actually
  // starts executing they will generally sample this time.
  Timer wake_;
};

} // namespace schtest::workloads
