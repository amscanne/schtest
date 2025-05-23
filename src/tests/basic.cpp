#include <gtest/gtest.h>
#include <thread>

#include "workloads/workloads.h"

using namespace schtest;
using namespace schtest::workloads;

TEST(Basic, PingPong) {
  TimeDistribution latency;
  Semaphore sem1(10);
  Semaphore sem2(10);
  std::thread t1([&] {
    for (int i = 0; i < 1000000; i++) {
      sem1.produce(latency, 1);
      sem2.consume(latency, 1);
      // std::cerr << "ping" << std::endl;
    }
  });
  std::thread t2([&] {
    for (int i = 0; i < 1000000; i++) {
      sem2.produce(latency, 1);
      sem1.consume(latency, 1);
      // std::cerr << "pong" << std::endl;
    }
  });
  t1.join();
  t2.join();
  std::cerr << latency;
}
