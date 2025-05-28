#include "workloads/process.h"
#include "util/output.h"
#include "workloads/context.h"

namespace schtest::workloads {

Result<> Process::start() {
  // First, create a cgroup which will hold this process.
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
    cgroup_.reset();
    auto r = std::move(final_result_.value());
    final_result_.reset();
    return r;
  }
  return OK();
}

} // namespace schtest::workloads
