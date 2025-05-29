#pragma once

#include "util/memfd.h"
#include "workloads/atomic.h"
#include "workloads/process.h"
#include "workloads/semaphore.h"

namespace schtest::workloads {

// Context controls the execution of a workload.
class Context {
public:
  // Creates a new context.
  //
  // Note that this may throw a runtime exception if any errors occur. The
  // context is typically where these errors are centralized and handled.
  static Context create();

  // Allocates shared memory for an object of the given type.
  //
  // The lifetime of this memory is bound to the context itself.
  template <typename T, typename... Args>
  T *allocate(Args &&...args) {
    size_t offset = offset_;
    if (offset % alignof(T) != 0) {
      offset += alignof(T) - (offset % alignof(T));
    }
    if (offset + sizeof(T) > mem_fd_.size()) {
      throw new std::runtime_error("out of memory");
    }
    auto *ptr = reinterpret_cast<T *>(mem_fd_.data() + offset);
    offset_ = offset + sizeof(T);
    return new (ptr) T(*this, std::forward<Args>(args)...);
  }

  // Adds a process to the given context.
  void add(std::function<Result<>()> fn) {
    processes_.emplace_back(allocate<Process>([this, fn]() {
      wait();
      return fn();
    }));
  }

  // Signal that the top-level test has started.
  Result<> start() {
    running_->store(true);
    for (size_t i = 0; i < processes_.size(); i++) {
      auto rv = processes_[i]->start();
      if (!rv) {
        running_->store(false);
        if (i != 0) {
          // Unwind all the previously started instances.
          wait_sem_->consume(i, i);
          start_sem_->produce(i, i);
          for (size_t j = 0; j < i; j++) {
            processes_[j]->join();
          }
        }
        // Propagate the error.
        return rv.takeError();
      }
    }

    // Signal everything to start.
    wait_sem_->consume(processes_.size());
    start_sem_->produce(processes_.size(), processes_.size());
    return OK();
  }

  // Whether the top-level test is running. This can be checked on each
  // completely iteration of the workload.
  bool running() const { return running_->load(); }

  // Signal that the test should be stopped.
  void stop() {
    if (running_->exchange(false)) {
      for (size_t i = 0; i < processes_.size(); i++) {
        processes_[i]->join();
      }
    }
  }

private:
  Context(MemFd &&mem_fd) : mem_fd_(std::move(mem_fd)) {
    running_ = allocate<Atomic<bool>>();
    wait_sem_ = allocate<Semaphore>();
    start_sem_ = allocate<Semaphore>();
  };

  // Callback from above for waiting to start execution.
  void wait() {
    wait_sem_->produce(1);
    start_sem_->consume(1);
  }

  MemFd mem_fd_;
  size_t offset_ = 0;

  // Always allocated in shared memory.
  std::vector<Process *> processes_;
  Atomic<bool> *running_;
  Semaphore *wait_sem_;
  Semaphore *start_sem_;
};

} // namespace schtest::workloads
