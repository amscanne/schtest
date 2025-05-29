#include <gtest/gtest.h>

#include "util/system.h"
#include "workloads/benchmark.h"
#include "workloads/spinner.h"

using namespace schtest;
using namespace schtest::workloads;

TEST(HyperThreads, SpreadingOut) {
  auto ctx = Context::create();

  // Jam everything onto one core. We have a number of workloads that equals our
  // total core count, but we don't spread it out at all. We expect that it
  // should spread out to be one per physical core, not one per logical core.
  auto system = System::load();
  ASSERT_TRUE(bool(system)) << system.takeError();

  // Construct our logical to physical core map.
  std::map<uint32_t, uint32_t> logical_to_physical;
  for (const auto &core : system->cores()) {
    for (const auto &hyperthread : core.hyperthreads()) {
      logical_to_physical[hyperthread.id()] = core.id();
    }
  }

  // Construct a set of threads, one per physical core.
  auto &cores = system->cores();
  std::vector<Spinner *> spinners;
  for (size_t i = 0; i < cores.size(); i++) {
    auto *spinner = ctx.allocate<Spinner>();
    spinners.push_back(spinner);
    ctx.add([i, &cores, spinner] {
      cores[i].migrate();
      return spinner->spin();
    });
  }

  // Our metric is the set of physical cores that are covered.
  std::function<double()> metric = [&] {
    std::map<uint32_t, uint32_t> counts;
    for (const auto &s : spinners) {
      counts[logical_to_physical[s->last_cpu()]]++;
    }
    return static_cast<double>(counts.size()) /
           static_cast<double>(system->cores().size());
  };

  EXPECT_GE(converge(ctx, metric, 0.95), 0.95);
}
