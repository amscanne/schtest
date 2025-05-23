#include <algorithm>
#include <fstream>
#include <sstream>

#include "sched/sched.h"

namespace schtest::sched {

Result<std::optional<std::string>> SchedExt::installed() {
  // Read the current state.
  std::fstream state("/sys/kernel/sched_ext/state", std::fstream::in);
  std::stringstream buffer;
  buffer << state.rdbuf();
  if (state.fail()) {
    return Error("failed to read /sys/kernel/sched_ext/state", errno);
  }

  // Strip newlines.
  std::string r = buffer.str();
  r.erase(std::remove(r.begin(), r.end(), '\n'), r.end());

  // If this disabled?
  if (r == "disabled") {
    return std::optional<std::string>();
  }
  // Is it on it's way? We won't hold our breath.
  if (r == "enabling") {
    return std::optional<std::string>();
  }
  // Is it something else?
  if (r != "enabled") {
    return Error("unexpected state: " + r, EINVAL);
  }

  // Read the name of the scheduler installed.
  std::fstream ops("/sys/kernel/sched_ext/root/ops", std::fstream::in);
  buffer.str("");
  buffer << ops.rdbuf();
  if (ops.fail()) {
    return Error("failed to read /sys/kernel/sched_ext/root/ops", errno);
  }

  // Read the scheduler & strip the newlines.
  r = buffer.str();
  r.erase(std::remove(r.begin(), r.end(), '\n'), r.end());

  return std::optional<std::string>(r);
}

} // namespace schtest::sched
