#include <cassert>
#include <cstdlib>
#include <fcntl.h>
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <thread>

#include "sched/sched.h"
#include "util/output.h"
#include "util/result.h"
#include "util/user.h"

using namespace schtest;
using namespace schtest::output;
using namespace schtest::sched;

struct ChildContext {
  // The notify_fd is the write end of a pipe when the child will ultimately
  // write the return value for the executed process. If the child is started
  // successully (on exec), then this will be closed automatically.
  int notify_fd;

  // These are pointers to the arguments to be passed to the child. These
  // locations must be valid until `notify_fd` is read above.
  char **argv;
};

class ParentContext {
public:
  ParentContext(pid_t pid) : child_(pid) {};

  // Determine if the child is still alive.
  bool alive() {
    if (!child_) {
      return false;
    }
    int status = 0;
    pid_t pid = waitpid(*child_, &status, WNOHANG);
    if ((pid == *child_ && (WIFEXITED(status) || WIFSIGNALED(status)))) {
      child_.reset();
      exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : -WTERMSIG(status);
      return false;
    }
    return kill(*child_, 0) == 0;
  }

  // Get the exit code of the child. This is valid only after `alive()` has
  // returned false at least once.
  int exit_code() {
    if (!exit_code_) {
      return 0;
    }
    return exit_code_.value();
  }

private:
  std::optional<pid_t> child_;
  std::optional<int> exit_code_;
};

static void child(ChildContext *ctx) {
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

static Result<ParentContext> spawn(char **argv) {
  std::cerr << "spawning: " << std::endl;
  for (int i = 0; argv[i]; i++) {
    std::cerr << " - " << argv[i] << std::endl;
  }

  // Create a pipe to notify the parent when the child has been started.
  int notify_pipe[2];
  int rc = pipe2(notify_pipe, O_CLOEXEC);
  if (rc < 0) {
    return Error("failed to create pipe", errno);
  }
  ChildContext ctx = {
      .notify_fd = notify_pipe[1],
      .argv = argv,
  };

  // Clone the child process.
  pid_t ret = (pid_t)syscall(__NR_clone, SIGCHLD | CLONE_NEWPID, NULL);
  if (ret < 0) {
    close(notify_pipe[0]);
    close(notify_pipe[1]);
    return Error("failed to clone", errno);
  }
  if (ret == 0) {
    close(notify_pipe[0]);
    child(&ctx);
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
      return ParentContext(ret);
    }
    assert(child_rc != 0);
    return Error("failed to start child", child_rc);
  }
}

const static char *usage =
    R"(schtest <flags> [--] [<binary> [flags...]]

This program drives a series of scheduler tests. It is used to drive simulated
workloads, and assert functional properties of the scheduler, which map to test
results that are emitted. There are also standardized benchmarks.

If an additional binary is given, it will be run safetly and the program will
ensure that a custom scheduer is installed before any tests are run. This requires
root privileges.
)";

static const char *flags = R"(
  Flags:
    -list
    -min_time (minimum time to run benchmarks) type: double default: 1.0
    -filter (list all available tests and benchmarks) type: string default: ""
    -helpfull (show full help message for all libraries and flags)
)";

DEFINE_bool(h, false, "show a short help messsage");
DEFINE_double(min_time, 1.0, "minimum time to run benchmarks");
DEFINE_string(filter, "",
              "filter to apply to the list of tests and benchmarks");
DEFINE_bool(list, false, "list all available tests and benchmarks");
DECLARE_string(helpmatch);
DECLARE_bool(help);

int main(int argc, char **argv) {
  gflags::SetUsageMessage(usage);
  gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
  if (FLAGS_help || FLAGS_h) {
    std::cout << argv[0] << ": " << usage;
    std::cout << flags;
    exit(1);
  }
  gflags::HandleCommandLineHelpFlags();

  // Build our custom google test flags.
  std::vector<std::string> gtest_flags;
  std::vector<const char *> gtest_argv;
  gtest_flags.push_back(argv[0]);
  if (FLAGS_list) {
    gtest_flags.push_back("--gtest_list_tests");
  }
  if (!FLAGS_filter.empty()) {
    gtest_flags.push_back("--gtest_filter=" + FLAGS_filter);
  }
  int gtest_argc = gtest_flags.size();
  for (const auto &flag : gtest_flags) {
    gtest_argv.push_back(flag.c_str());
  }
  gtest_argv.push_back(nullptr);
  testing::InitGoogleTest(&gtest_argc, const_cast<char **>(gtest_argv.data()));

  if (argc > 1) {
    // We require root privileges to install a custom scheduler.
    if (!User::is_root()) {
      std::cerr << "error: must run as root to install a custom scheduler"
                << std::endl;
      return 1;
    }

    // If we are spawning a subprocess, check to see that there is no
    // current sched-ext process installed. We will run one safely, and then
    // wait for it to be installed.
    auto scheduler = SchedExt::installed();
    if (!scheduler) {
      std::cerr << "error: unable to query scheduler: " << scheduler.takeError()
                << std::endl;
      return 1;
    }
    if (scheduler->has_value()) {
      std::cerr << "error: scheduler already installed: " << scheduler->value()
                << std::endl;
      return 1;
    }

    // Run the given binary in its own pid namespace, to ensure that it can
    // be fully captured (it may daemonize, etc.).
    auto child = spawn(argv + 1);
    if (!child) {
      std::cerr << "error: unable to spawn: " << child.takeError() << std::endl;
      return 1;
    }

    // Wait for a custom scheduler to be installed.
    std::cerr << "waiting for scheduler to be installed... ";
    std::cerr.flush();
    while (true) {
      auto scheduler = SchedExt::installed();
      if (!scheduler) {
        std::cerr << std::endl;
        std::cerr << "error: unable to query scheduler: "
                  << scheduler.takeError() << std::endl;
        return 1;
      }
      if (!child->alive()) {
        std::cerr << std::endl;
        std::cerr << "error: child exited with code " << child->exit_code()
                  << " before installing scheduler" << std::endl;
        return 1;
      }
      if (scheduler->has_value()) {
        std::cerr << scheduler->value() << std::endl;
        break;
      }
      // Wait for a while to see if it starts up.
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  testing::UnitTest &unit_test = *testing::UnitTest::GetInstance();
  testing::TestEventListeners &listeners = unit_test.listeners();

  // Removes the default console output listener from the list so it will
  // not receive events from Google Test and won't print any output. We transfer
  // ownership to our own singleton object, which is capable of producing other
  // kinds of output.
  std::unique_ptr<testing::TestEventListener> output(
      listeners.Release(listeners.default_result_printer()));
  testing::TestEventListener *custom = Output::setup(std::move(output));
  listeners.Append(custom);

  // Run all tests.
  return RUN_ALL_TESTS();
}
