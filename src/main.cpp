#include <cassert>
#include <cstdlib>
#include <fcntl.h>
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <thread>

#include "sched/sched.h"
#include "util/child.h"
#include "util/output.h"
#include "util/result.h"
#include "util/user.h"

using namespace schtest;
using namespace schtest::output;
using namespace schtest::sched;

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

  // We require root privileges to create cgroups, install the custom scheduler.
  if (!User::is_root()) {
    std::cerr << "error: must run as root" << std::endl;
    return 1;
  }

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

  std::optional<Child> child_proc; // For lifecycle.
  if (argc > 1) {
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
    std::cerr << "spawning: " << std::endl;
    for (int i = 1; argv[i]; i++) {
      std::cerr << " - " << argv[i] << std::endl;
    }
    auto child = Child::spawn(argv + 1);
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

    // Push the child into the outer scope.
    child_proc.emplace(std::move(*child));
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
  int rc = RUN_ALL_TESTS();

  // Kill the scheduler, if there was one running.
  if (child_proc) {
    child_proc->kill(SIGKILL);
  }

  return rc;
}
