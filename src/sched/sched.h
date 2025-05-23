#pragma once

#include <optional>
#include <string>

#include "util/result.h"

namespace schtest::sched {

// SchedExt is a class that wraps iteractions with the underlying sched-ext
// subsystem. Generally this is informational.
class SchedExt {
public:
  static Result<std::optional<std::string>> installed();
};

} // namespace schtest::sched
