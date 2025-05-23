#include <gtest/gtest.h>
#include <thread>

#include "util/benchmark.h"
#include "util/stats.h"
#include "workloads/workloads.h"

using namespace schtest;
using namespace schtest::workloads;
using namespace schtest::benchmark;

TEST(Basic, PingPong) {
  auto latency = run<std::chrono::nanoseconds>(
      std::cerr, 10000,
      [&](size_t iters, Distribution<std::chrono::nanoseconds> &latency) {
        Semaphore sem1(10);
        Semaphore sem2(10);
        std::thread t1([&] {
          for (int i = 0; i < iters; i++) {
            sem1.produce(latency, 1);
            sem2.consume(latency, 1);
          }
        });
        std::thread t2([&] {
          for (int i = 0; i < iters; i++) {
            sem2.produce(latency, 1);
            sem1.consume(latency, 1);
          }
        });
        t1.join();
        t2.join();
      });
  std::cerr << *latency << std::endl;
}
