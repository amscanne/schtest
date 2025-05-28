#pragma once

#include <functional>

#include "workloads/context.h"

namespace schtest::workloads {

class Spinner {
public:
  Spinner(Context &ctx,
          std::chrono::duration<double> duration = std::chrono::years(99))
      : ctx_(ctx), duration_(duration){};
  Spinner(const Spinner &other) = delete;

  // Spins directly, for use in other functions.
  Result<> spin();

  // The last CPU that this spinner ran on.
  uint32_t last_cpu() const { return cpu_id_.load(); }

private:
  Context &ctx_;
  std::chrono::duration<double> duration_;
  std::atomic<uint32_t> cpu_id_;
};

} // namespace schtest::workloads
