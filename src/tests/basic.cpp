#include <gtest/gtest.h>
#include <thread>

#include "util/stats.h"
#include "util/system.h"
#include "workloads/benchmark.h"
#include "workloads/context.h"
#include "workloads/semaphore.h"
#include "workloads/spinner.h"

using namespace schtest;
using namespace schtest::workloads;

TEST(Basic, PingPong) {
  auto ctx = Context::create();

  auto *sem1 = ctx.allocate<Semaphore>();
  auto *sem2 = ctx.allocate<Semaphore>();
  ctx.add([&] {
    while (ctx.running()) {
      sem1->produce(1);
      sem2->consume(1);
    }
    sem1->produce(1);
    return OK();
  });
  ctx.add([&] {
    size_t iters = 0;
    while (ctx.running()) {
      sem2->produce(1);
      sem1->consume(1);
    }
    sem2->produce(1);
    return OK();
  });
  benchmark(ctx, [&]() {
    sem1->reset();
    sem2->reset();
    Distribution<std::chrono::nanoseconds> wake_latency;
    sem1->flush(wake_latency);
    sem2->flush(wake_latency);
    return wake_latency.estimates();
  });
}

TEST(Basic, Workers) {
  auto ctx = Context::create();

  auto system = System::load();
  ASSERT_TRUE(bool(system)) << system.takeError();

  size_t worker_count = system->logical_cpus();
  auto *outbound = ctx.allocate<Semaphore>();
  auto *inbound = ctx.allocate<Semaphore>();
  ctx.add([&] {
    std::vector<std::thread> workers;
    for (size_t i = 0; i < worker_count; i++) {
      workers.emplace_back([&] {
        while (ctx.running()) {
          Spinner work(ctx, std::chrono::microseconds(10));
          outbound->consume(1);
          work.spin();
          inbound->produce(1);
        }
      });
    }
    // Wake up all threads.
    outbound->produce(worker_count, worker_count);
    while (ctx.running()) {
      inbound->consume(1);
      outbound->produce(1);
    }
    // Wake up anything still blocked.
    outbound->produce(worker_count, worker_count);
    for (auto &t : workers) {
      t.join();
    }
    return OK();
  });
  benchmark(ctx, [&]() {
    outbound->reset();
    inbound->reset();
    Distribution<std::chrono::nanoseconds> wake_latency;
    outbound->flush(wake_latency);
    inbound->flush(wake_latency);
    return wake_latency.estimates();
  });
}

class HerdTest : public testing::TestWithParam<int> {};

TEST_P(HerdTest, WakeUps) {
  int count = GetParam();
  auto ctx = Context::create();

  Semaphore *outbound = ctx.allocate<Semaphore>();
  Semaphore *inbound = ctx.allocate<Semaphore>();

  // There just a single process in this setup, which has its own collection of
  // threads. This process does a broadcast to all child threads, then collects
  // some result from each one of them in turn.
  ctx.add([&] {
    // Create all process-local threads.
    std::atomic<bool> threads_running = true;
    std::vector<std::thread> threads;
    for (int i = 0; i < count; i++) {
      threads.emplace_back([&threads_running, &outbound, &inbound] {
        while (threads_running.load()) {
          outbound->consume(1);
          inbound->produce(1);
        }
      });
    }

    // Broadcast and collect.
    while (ctx.running()) {
      for (int i = 0; i < count; i++) {
        outbound->produce(count);
        inbound->consume(count);
      }
    }

    // Unblock all threads.
    threads_running.store(false);
    outbound->produce(2 * count, count);
    for (int i = 0; i < count; i++) {
      threads[i].join();
    }
    return OK();
  });
  benchmark(ctx, [&]() {
    outbound->reset();
    inbound->reset();
    Distribution<std::chrono::nanoseconds> wake_latency;
    outbound->flush(wake_latency);
    return wake_latency.estimates();
  });
}

INSTANTIATE_TEST_SUITE_P(Broadcast, HerdTest, ::testing::Values(1, 2, 4, 8, 16),
                         testing::PrintToStringParamName());
