#include "util/result.h"

namespace schtest {

Error::~Error() {
  if (!checked_) {
    // This is a fatal error.
    std::cerr << "Error destructor called without checking: " << *this
              << std::endl;
  }
}

} // namespace schtest
