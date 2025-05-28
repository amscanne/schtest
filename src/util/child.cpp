#include <cassert>
#include <cstdlib>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "util/child.h"

namespace schtest {

struct ExecContext {
  // The notify_fd is the write end of a pipe when the child will ultimately
  // write the return value for the executed process. If the child is started
  // successully (on exec), then this will be closed automatically.
  int notify_fd;

  // These are pointers to the arguments to be passed to the child. These
  // locations must be valid until `notify_fd` is read above.
  char **argv;
};

static void exec_child(ExecContext *ctx) {
  // Ensure that we die when the child dies.
  int rc = prctl(PR_SET_PDEATHSIG, SIGKILL, 0, 0, 0);
  if (rc < 0) {
    assert(write(ctx->notify_fd, &rc, sizeof(rc)) == 1);
    assert(close(ctx->notify_fd) == 0);
    exit(1);
  }

  // Spawn the given child process.
  pid_t child = fork();
  if (child < 0) {
    assert(write(ctx->notify_fd, &child, sizeof(child)) == 1);
    assert(close(ctx->notify_fd) == 0);
    exit(1);
  }
  if (child == 0) {
    // There is no need to protect with PR_SET_PDEATHSIG here, as the new
    // init in the namespace will die when the parent dies, and the entire
    // namespace will be killed. This covers the case when the child here
    // attempts ot daemonize or anything else like that.
    rc = execvp(*ctx->argv, ctx->argv);
    assert(write(ctx->notify_fd, &child, sizeof(child)) == 0);
    assert(close(ctx->notify_fd) == 0);
    exit(1);
  }

  // The child will either write to the notify pipe, or close it. Either
  // way, we can safely close it here.
  close(ctx->notify_fd);

  // Reap processes.
  while (true) {
    // Wait for anything to exit, it may have been reparented to us.
    int status;
    pid_t pid = waitpid(-1, &status, 0);

    // Check to see if it was the original child. If yes, then we will
    // exit as well to notify the parent that are done.
    if (pid == child) {
      if (WIFEXITED(status)) {
        exit(WEXITSTATUS(status));
      } else if (WIFSIGNALED(status)) {
        exit(255);
      }
    }
  }
}

Result<Child> Child::run(std::function<void()> fn, unsigned long extra_flags) {
  // Create a regular child process.
  pid_t ret = (pid_t)syscall(__NR_clone, SIGCHLD | extra_flags, NULL);
  if (ret == 0) {
    fn();
    exit(0);
  }
  if (ret < 0) {
    return Error("failed to clone", errno);
  }
  return Child(ret);
}

Result<Child> Child::spawn(char **argv) {
  // Create a pipe to notify the parent when the child has been started.
  int notify_pipe[2];
  int rc = pipe2(notify_pipe, O_CLOEXEC);
  if (rc < 0) {
    return Error("failed to create pipe", errno);
  }
  ExecContext ctx = {
      .notify_fd = notify_pipe[1],
      .argv = argv,
  };
  auto res = run(
      [&]() {
        close(notify_pipe[0]);
        exec_child(&ctx);
      },
      CLONE_NEWPID);
  if (!res) {
    close(notify_pipe[0]);
    close(notify_pipe[1]);
    return res.takeError();
  }

  // Wait for the child to be started.
  close(notify_pipe[1]);
  while (true) {
    int child_rc = 0;
    rc = read(notify_pipe[0], &child_rc, sizeof(child_rc));
    if (rc < 0 && errno == EINTR) {
      continue;
    }
    close(notify_pipe[0]);
    if (rc == 0) {
      return std::move(res);
    }
    assert(child_rc != 0);
    return Error("failed to start child", child_rc);
  }
}

bool Child::wait(bool block) {
  if (!child_) {
    return false;
  }
  int status = 0;
  int flags = 0;
  if (!block) {
    flags |= WNOHANG;
  }
  pid_t pid = waitpid(*child_, &status, flags);
  if ((pid == *child_ && (WIFEXITED(status) || WIFSIGNALED(status)))) {
    child_.reset();
    exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : -WTERMSIG(status);
    return false;
  }
  return kill(0);
}

bool Child::kill(int sig) { return ::kill(*child_, sig) == 0; }

} // namespace schtest
