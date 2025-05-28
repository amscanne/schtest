#pragma once

#include "util/clock.h"
#include "util/result.h"
#include "util/stats.h"
#include "workloads/context.h"

namespace schtest::workloads {

class Context;

// Socketpair is a simple synchronization primitive that relies on a pair
// of connected sockets.
class SocketPair {
public:
  ~SocketPair() {
    close(read_fd_);
    close(write_fd_);
  };
  static Result<SocketPair> create(Context &ctx, uint32_t max);
  SocketPair(SocketPair &&) = default;
  SocketPair(const SocketPair &) = delete;

  void consume(uint32_t v);
  void produce(uint32_t v);

private:
  SocketPair(Context &ctx, int read_fd, int write_fd)
      : ctx_(ctx), read_fd_(read_fd), write_fd_(write_fd){};

  Context &ctx_;
  int read_fd_;
  int write_fd_;

  // The `wake_` field tracks time spent in blocking calls.
  Timer wake_;
};

} // namespace schtest::workloads
