#pragma once

#include <thread>

#include "util/workloads.h"

namespace schtest::workloads {

class Thread : public std::thread {
public:
  Thread(Control &control, Work &&work)
      : std::thread([this] { work(); }), control_(control),
        work_(std::move(work)), cpu_id_(std::numeric_limits<uint32_t>::max()) {}

  // Returns the last cpu_id that this thread was assigned to. Note that
  // it is not possible to assert anything about the exact current CPU.
  uint32_t cpu_id() const { return cpu_id_.load(); }

private:
  void work() {
    while (control_.wait()) {
      work_.run();
    }
  }

  Control &control_;
  Work work_;
  std::atomic<uint32_t> cpu_id_;
};

} // namespace schtest::workloads
