#include <gtest/gtest.h>
#include <string>

#include "util/cgroups.h"

using namespace schtest;

TEST(Cgroups, CreateAndDestroy) {
  // Generate a unique name for the cgroup
  std::string cgroup_name = "test_cgroup_" + std::to_string(getpid());

  // Create a new cgroup
  auto cgroup_result = Cgroup::create(cgroup_name);
  ASSERT_TRUE(cgroup_result)
      << "Failed to create cgroup: " << cgroup_result.takeError();

  // Get the cgroup object
  Cgroup cgroup = std::move(*cgroup_result);

  // Verify that the cgroup path exists
  std::filesystem::path cgroup_path = cgroup.path();
  ASSERT_TRUE(std::filesystem::exists(cgroup_path))
      << "Cgroup path does not exist: " << cgroup_path;

  // The cgroup will be automatically destroyed when it goes out of scope
}

TEST(Cgroups, MoveSemantics) {
  // Generate a unique name for the cgroup
  std::string cgroup_name = "test_cgroup_move_" + std::to_string(getpid());

  // Create a new cgroup
  auto cgroup_result = Cgroup::create(cgroup_name);
  ASSERT_TRUE(cgroup_result);

  // Get the cgroup object
  Cgroup cgroup1 = std::move(*cgroup_result);
  std::filesystem::path cgroup_path = cgroup1.path();

  // Move the cgroup to another object
  Cgroup cgroup2 = std::move(cgroup1);

  // Verify that the path was moved
  ASSERT_TRUE(cgroup1.path().empty());
  ASSERT_EQ(cgroup2.path(), cgroup_path);

  // Verify that the cgroup path still exists
  ASSERT_TRUE(std::filesystem::exists(cgroup_path));

  // The cgroup will be automatically destroyed when cgroup2 goes out of scope
}

TEST(Cgroups, ErrorHandling) {
  // Try to create a cgroup with an invalid name
  auto cgroup_result = Cgroup::create("/invalid/name");

  // This should fail
  ASSERT_FALSE(cgroup_result);

  // Take the error and verify it's not empty
  Error error = cgroup_result.takeError();
  std::stringstream ss;
  ss << error;
  ASSERT_FALSE(ss.str().empty());
}
