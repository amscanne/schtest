#include "util/result.h"

namespace schtest {

Error::~Error() {
  if (!checked_) {
    // This is a fatal error.
    std::cerr << "Error destructor called without checking." << std::endl;
    std::cerr << *this << std::endl;
    std::terminate();
  }
}

} // namespace schtest
