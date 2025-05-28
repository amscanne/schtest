#include "util/memfd.h"

namespace schtest {

Result<MemFd> MemFd::create(const std::string &name, size_t size) {
  size_t page_size = getpagesize();
  size_t rounded_up = (size + page_size - 1) & ~(page_size - 1);
  int fd = memfd_create(name.c_str(), 0);
  if (fd == -1) {
    return Error("failed to create memfd", errno);
  }
  if (ftruncate(fd, rounded_up) < 0) {
    close(fd);
    return Error("ftruncate failed", errno);
  }
  void *data =
      mmap(nullptr, rounded_up, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  return MemFd(fd, data, rounded_up);
}

MemFd::~MemFd() {
  if (data_ != nullptr) {
    munmap(data_, size_);
  }
  if (fd_ != -1) {
    close(fd_);
  }
}

} // namespace schtest
