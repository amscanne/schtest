#pragma once

#include "util/cgroups.h"
#include "util/child.h"
#include "workloads/semaphore.h"

namespace schtest::workloads {

class Context;

class Process {
public:
  template <typename... RV>
  Process(Context &ctx, std::function<RV()>... fn)
      : Process(ctx, [fn...] { return eval(fn...); }){};
  Process(Context &ctx, std::function<Result<>(void)> fn)
      : ctx_(ctx), fn_(fn), start_(ctx){};
  Process(Process &&other) = delete;
  Process(const Process &other) = delete;
  ~Process() { join(); };

  // Start starts the given process.
  Result<> start();

  // Join waits for the process to exit.
  Result<> join();

private:
  template <typename T, typename... R>
  static Result<> eval(std::function<T(void)> fn,
                       std::function<R(void)>... others) {
    auto ok = fn();
    if (!ok) {
      return ok.takeError();
    }
    return eval(others...);
  }

  Context &ctx_;
  std::function<Result<>()> fn_;
  std::optional<Cgroup> cgroup_;
  std::optional<Child> child_;
  Semaphore start_;
  std::optional<Result<>> start_result_;
  std::optional<Result<>> final_result_;
};

} // namespace schtest::workloads
