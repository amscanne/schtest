#pragma once

#include <unistd.h>

namespace schtest {

class User {
public:
  // Simply check whether the current user is root.
  static bool is_root() { return geteuid() == 0; }
};

} // namespace schtest