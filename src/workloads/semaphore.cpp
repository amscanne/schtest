#include <linux/futex.h>

#include "workloads/semaphore.h"

namespace schtest::workloads {

// waiter is a flag that will be attached to the semaphore in
// order to indicate that there are active waiters.
constexpr uint32_t consumer_waiter = 0x80000000ul;
constexpr uint32_t producer_waiter = 0x40000000ul;

void Semaphore::consume(uint32_t v, uint32_t wake) {
  uint32_t cur = count_.load(std::memory_order_acquire);
  while (true) {
    uint32_t amount = cur & ~(producer_waiter | consumer_waiter);
    if (amount >= v) {
      bool has_waiter = (cur & producer_waiter) == producer_waiter;
      if (count_.compare_exchange_weak(cur, amount - v,
                                       std::memory_order_acq_rel)) {
        if (has_waiter) {
          wake_.reset();
          syscall(SYS_futex, &count_, FUTEX_WAKE, wake, nullptr, nullptr, 0);
        }
        return;
      }
    } else {
      // We don't really have any better mechanism for synchronizing than
      // raw futuxes. We could theoretically construct a bunch of sockets
      // and other things here, but this is likely to be the faster, and
      // won't trigger any kind of integer heuristics in the kernel.
      bool has_waiter = (cur & consumer_waiter) == consumer_waiter;
      if (!has_waiter) {
        if (!count_.compare_exchange_weak(cur, cur | consumer_waiter,
                                          std::memory_order_acq_rel)) {
          continue;
        }
        cur |= consumer_waiter;
      }
      auto cookie = wake_.cookie();
      int rc =
          syscall(SYS_futex, &count_, FUTEX_WAIT, cur, nullptr, nullptr, 0);
      if (rc == 0) {
        if (auto v = wake_.elapsed(cookie)) {
          sample(*v);
        }
      }
      cur = count_.load(std::memory_order_acquire);
    }
  }
}

void Semaphore::produce(uint32_t v, uint32_t wake) {
  uint32_t cur = count_.load(std::memory_order_acquire);
  while (true) {
    uint32_t amount = cur & ~(producer_waiter | consumer_waiter);
    if (amount + v <= max_) {
      bool has_waiter = (cur & consumer_waiter) == consumer_waiter;
      if (count_.compare_exchange_weak(cur, amount + v,
                                       std::memory_order_acq_rel)) {
        if (has_waiter) {
          wake_.reset();
          syscall(SYS_futex, &count_, FUTEX_WAKE, wake, nullptr, nullptr, 0);
        }
        return;
      }
    } else {
      // See above.
      bool has_waiter = (cur & producer_waiter) == producer_waiter;
      if (!has_waiter) {
        if (!count_.compare_exchange_weak(cur, cur | producer_waiter,
                                          std::memory_order_acq_rel)) {
          continue;
        }
        cur |= producer_waiter;
      }
      auto cookie = wake_.cookie();
      int rc = syscall(SYS_futex, &count_, FUTEX_WAIT, 0, nullptr, nullptr, 0);
      if (rc == 0) {
        if (auto v = wake_.elapsed(cookie)) {
          sample(*v);
        }
      }
    }
    cur = count_.load(std::memory_order_acquire);
  }
}

} // namespace schtest::workloads
