#pragma once

#include <cassert>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <sched.h>
#include <sys/wait.h>

#include "util/result.h"

namespace schtest {

// Child is a wrapper around a child process.
//
// This class is used to spawn processes both for the purposes of executing some
// other process, as well as for the purposes of wrapping internal execution
// within another process.
class Child {
public:
  ~Child() {
    bool running = wait(true);
    assert(!running);
  }
  Child(Child &&other)
      : child_(std::move(other.child_)),
        exit_code_(std::move(other.exit_code_)) {
    other.child_.reset();
  }
  Child &operator=(Child &&other) = delete;
  Child(const Child &other) = delete;
  Child &operator=(const Child &other) = delete;

  // Start a child process, and execute the given function in the child.
  static Result<Child> run(std::function<void()> fn,
                           unsigned long extra_flags = 0);

  // Start a child process, and execute the given command in the child.
  //
  // In this case, the child will always be safely started in a separate PID
  // namespace, to ensure that all subprocesses are effectively died together in
  // their lifecycle.
  static Result<Child> spawn(char **argv);

  // Determine if the child is still alive.
  bool alive() { return wait(false); }

  // Waits for the child to exit.
  bool wait(bool block = true);

  // Sends a signal to the child.
  bool kill(int sig = SIGKILL);

  // Get the exit code of the child. This is valid only after `alive()` has
  // returned false at least once.
  int exit_code() {
    if (!exit_code_) {
      return 0;
    }
    return exit_code_.value();
  }

private:
  Child(pid_t pid) : child_(pid){};

  std::optional<pid_t> child_;
  std::optional<int> exit_code_;
};

} // namespace schtest
