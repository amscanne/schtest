#include "workloads/context.h"

namespace schtest::workloads {

Context Context::create() {
  auto mem_fd = MemFd::create("allocator", 1024 * 1024 * 1024);
  if (!mem_fd) {
    std::stringstream ss;
    ss << mem_fd.takeError();
    throw std::runtime_error(ss.str());
  }
  return Context(std::move(*mem_fd));
}

} // namespace schtest::workloads
