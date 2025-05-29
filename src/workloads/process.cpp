#include <sys/prctl.h>

#include "workloads/process.h"

namespace schtest::workloads {

Result<> Process::start() {
  // First, create a cgroup which will hold this process. This may already
  // exist, in which case we can simply reuse the group.
  if (!cgroup_) {
    auto cg = Cgroup::create();
    if (!cg) {
      return cg.takeError();
    }
    cgroup_.emplace(std::move(*cg));
  }

  // Create the child process. Everything that we want to share must have been
  // shared explicitly through the context allocator.
  auto fn = [this] {
    auto result = cgroup_->enter();
    if (!result) {
      start_result_.emplace(result.takeError());
      start_.produce(1);
      return;
    }

    // Opt-in to the extensible scheduler class.
    struct sched_param param = {
        .sched_priority = priority_,
    };
    if (sched_setscheduler(0, 7 /* SCHED_EXT */, &param) < 0) {
      start_result_.emplace(Error("failed to set scheduler policy", errno));
      start_.produce(1);
      return;
    }

    // Set our process name.
    if (!name_.empty() &&
        prctl(PR_SET_NAME, name_.c_str(), nullptr, nullptr, nullptr) < 0) {
      start_result_.emplace(Error("failed to set process name", errno));
      start_.produce(1);
      return;
    }

    // Run the given function.
    start_result_.emplace(std::move(*result));
    start_.produce(1);
    final_result_.emplace(fn_());
  };
  auto child = Child::run(fn);
  if (!child) {
    return child.takeError();
  }
  child_.emplace(std::move(*child));
  start_.consume(1);
  auto r = std::move(start_result_.value());
  start_result_.reset();
  return r;
}

Result<> Process::join() {
  if (child_) {
    child_->wait();
    child_.reset();
    if (final_result_.has_value()) {
      auto r = std::move(final_result_.value());
      final_result_.reset();
      return r;
    }
  }
  return OK();
}

} // namespace schtest::workloads
