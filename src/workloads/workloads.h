#pragma once

#include <atomic>
#include <concepts>
#include <utility>

#include "util/clock.h"

namespace schtest::workloads {

// Semaphore is a basic counting semaphore, that will block and unblock
// thread execution that hits it. Calling `consume` will block the current
// thread until the semaphore has sufficient count available, which is done by
// calling `produce`. If `produce` is called ahead of `consume`, then the thread
// will be unblocked immediately and not wait.
//
// To avoid any additional contention, all operations on the semaphore
// are lock-free. The type is not copyable or movable, because the we
// use the address of the `count_` to synchronize threads.
class Semaphore {
public:
  Semaphore(uint32_t max) : max_(max){};
  ~Semaphore() = default;
  Semaphore(const Semaphore &) = delete;
  Semaphore(Semaphore &&) = delete;
  Semaphore &operator=(const Semaphore &) = delete;
  Semaphore &operator=(Semaphore &&) = delete;

  void consume(TimeDistribution &latency, uint32_t v, uint32_t wake = 1);
  void produce(TimeDistribution &latency, uint32_t v, uint32_t wake = 1);

private:
  std::atomic<uint32_t> count_ = 0;
  const uint32_t max_;

  // The `wake_` field tracks when the semaphore was notifier (either for
  // producer or consumer), and when the thread that was notifier actually
  // starts executing they will generally sample this time.
  Timer wake_;
};

// Work is a general interface for a workload. A workload is a thing that
// completes work and then finishes.
class Work {
public:
  virtual ~Work() = 0;
};

template <typename T>
concept WorkType = std::derived_from<T, Work>;

// Sequence is a specific kind of work that runs in a sequence.
template <WorkType... Ts>
class Sequence : public Work {
public:
  Sequence(Ts... seq) : seq_(std::move(seq)...){};
  ~Sequence() override = default;

private:
  std::tuple<Ts...> seq_;
};

// Cycle is a specific kind of work, that repeats in a cycle.
template <WorkType... Ts>
class Cycle : public Work {
public:
  Cycle(Ts... seq) : seq_(std::move(seq)...){};
  ~Cycle() override = default;

private:
  std::tuple<Ts...> seq_;
};

// Dispatcher is something that takes work and gets it run.
class Dispatcher {
public:
};

// Thread is a basic unit of execution, which is given some work to do.
template <WorkType W>
class Thread {
public:
  Thread(W work) : work_(std::move(work)){};

private:
  W work_;
};

// StaticThreadPool is a thread pool that hands work to a fixed number of
// threads.
template <std::size_t N>
class StaticThreadPool {
public:
};

// DynamicThreadPool is a thread pool that creates a new thread for each new
// work item.
template <WorkType W>
class DynamicThreadPool {
public:
};

} // namespace schtest::workloads
