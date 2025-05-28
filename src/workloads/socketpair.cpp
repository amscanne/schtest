#include <sys/socket.h>
#include <sys/types.h>

#include "util/result.h"
#include "workloads/context.h"
#include "workloads/socketpair.h"

namespace schtest::workloads {

Result<SocketPair> SocketPair::create(Context &ctx, uint32_t max) {
  int fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
    return Error("unable to create socket", errno);
  }
  if (setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &max, sizeof(max)) < 0) {
    return Error("unable to set send buffer size", errno);
  }
  if (setsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &max, sizeof(max)) < 0) {
    return Error("unable to set receive buffer size", errno);
  }
  return SocketPair(ctx, fds[0], fds[1]);
}

void SocketPair::consume(uint32_t v) {}

void SocketPair::produce(uint32_t v) {}

} // namespace schtest::workloads
