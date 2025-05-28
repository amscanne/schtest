#include "workloads/spinner.h"
#include "util/clock.h"

namespace schtest::workloads {

Result<> Spinner::spin() {
  Timer timer;
  while (ctx_.running() && timer.elapsed() < duration_) {
    uint32_t cpu = 0;
    if (getcpu(&cpu, nullptr) == 0) {
      cpu_id_.store(cpu);
    }
  };
  return OK();
}

} // namespace schtest::workloads
