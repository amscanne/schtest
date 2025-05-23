#include <gtest/gtest.h>
#include <map>
#include <thread>
#include <type_traits>

#include "util/system.h"

using namespace schtest;

struct SpinControlBlock {
  uint32_t cpu_id;
  volatile bool running;
};

void spin(SpinControlBlock *ptr) {
  while (ptr->running) {
    unsigned int cpu = ptr->cpu_id;
    if (getcpu(&cpu, nullptr) == 0) {
      ptr->cpu_id = cpu;
    }
  };
}

TEST(HyperThreads, SpreadingOut) {
  // Jam everything onto one core. We have a number of workloads that equals our
  // total core count, but we don't spread it out at all. We expect that it
  // should spread out to be one per physical core, not one per logical core.
  volatile bool running = true;
  std::vector<std::unique_ptr<SpinControlBlock>> scbs;
  std::vector<std::thread> threads;
  auto system = System::load();

  std::cerr << *system << std::endl;
  auto &cores = system->cores();
  for (size_t i = 0; i < cores.size(); i++) {
    auto &scb = scbs.emplace_back(std::make_unique<SpinControlBlock>());
    scb->running = true;

    auto &core = cores[0]; // Always the first one.
    threads.emplace_back([ptr = scb.get(), core] {
      core.migrate();
      spin(ptr);
    });
  }

  // Construct our logical to physical core map.
  std::map<uint32_t, uint32_t> logical_to_physical;
  for (const auto &core : system->cores()) {
    for (const auto &hyperthread : core.hyperthreads()) {
      logical_to_physical[hyperthread.id()] = core.id();
    }
  }

  // Map to the unique cores.
  while (true) {
    std::map<uint32_t, uint32_t> counts;
    for (const auto &scb : scbs) {
      counts[logical_to_physical[scb->cpu_id]]++;
    }
    std::cerr << "Cores: " << counts.size() << ", "
              << "Physical cores: " << system->cores().size() << ", "
              << "Threads: " << threads.size() << std::endl;
    if (counts.size() == system->cores().size()) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  for (auto &scb : scbs) {
    scb->running = false;
  }
  for (auto &thread : threads) {
    thread.join();
  }
}
