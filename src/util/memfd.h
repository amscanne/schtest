#pragma once

#include <cstddef>
#include <sys/mman.h>
#include <unistd.h>

#include "util/result.h"

namespace schtest {

class MemFd {
public:
  static Result<MemFd> create(const std::string &name, size_t size);
  ~MemFd();
  MemFd(MemFd &&other)
      : fd_(other.fd_), data_(other.data_), size_(other.size_) {
    other.fd_ = -1;
    other.data_ = nullptr;
    other.size_ = 0;
  }
  MemFd &operator=(MemFd &&other) = delete;
  MemFd(const MemFd &) = delete;
  MemFd &operator=(const MemFd &) = delete;

  size_t size() const { return size_; }
  char *data() const { return static_cast<char *>(data_); }

private:
  MemFd(int fd, void *data, size_t size) : fd_(fd), data_(data), size_(size) {}

  int fd_ = -1;
  void *data_ = nullptr;
  size_t size_ = 0;
};

} // namespace schtest
