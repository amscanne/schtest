#include "util/system.h"
#include <gtest/gtest.h>
#include <vector>

using namespace schtest;

TEST(HyperthreadTest, ConstructorAndId) {
  int id = 3;
  Hyperthread ht(id);
  EXPECT_EQ(ht.id(), id);
}

TEST(CoreTest, ConstructorAndAccessors) {
  int core_id = 1;
  std::vector<Hyperthread> hts;
  hts.emplace_back(0);
  hts.emplace_back(1);
  Core core(core_id, std::move(hts));
  EXPECT_EQ(core.id(), core_id);
  EXPECT_EQ(core.hyperthreads().size(), 2);
  EXPECT_EQ(core.hyperthreads()[0].id(), 0);
  EXPECT_EQ(core.hyperthreads()[1].id(), 1);
}

TEST(CoreComplexTest, ConstructorAndAccessors) {
  int cc_id = 2;
  std::vector<Hyperthread> hts;
  hts.emplace_back(0);
  hts.emplace_back(1);
  std::vector<Core> cores;
  cores.emplace_back(0,
                     std::vector<Hyperthread>{Hyperthread(0), Hyperthread(1)});
  cores.emplace_back(1,
                     std::vector<Hyperthread>{Hyperthread(2), Hyperthread(3)});
  CoreComplex cc(cc_id, std::move(cores));
  EXPECT_EQ(cc.id(), cc_id);
  EXPECT_EQ(cc.cores().size(), 2);
  EXPECT_EQ(cc.cores()[0].id(), 0);
  EXPECT_EQ(cc.cores()[1].id(), 1);
}

TEST(NodeTest, ConstructorAndAccessors) {
  int node_id = 0;
  std::vector<CoreComplex> complexes;
  std::vector<Core> cores1;
  cores1.emplace_back(0,
                      std::vector<Hyperthread>{Hyperthread(0), Hyperthread(1)});
  complexes.emplace_back(0, std::move(cores1));
  Node node(node_id, std::move(complexes));
  EXPECT_EQ(node.id(), node_id);
  EXPECT_EQ(node.complexes().size(), 1);
  EXPECT_EQ(node.complexes()[0].id(), 0);
}

TEST(SystemTest, SystemLoadAndAccessors) {
  auto sys_result = System::load();
  ASSERT_TRUE(sys_result.ok());
  const System &sys = *sys_result;
  EXPECT_GE(sys.nodes().size(), 1);
  EXPECT_GE(sys.cores().size(), 1);
  EXPECT_GE(sys.logical_cpus(), 1);
}

// Test operator<< overloads (smoke test)
TEST(SystemTest, OstreamOperators) {
  auto sys_result = System::load();
  ASSERT_TRUE(sys_result.ok());
  const System &sys = *sys_result;
  std::ostringstream oss;
  oss << sys;
  SUCCEED();
}
