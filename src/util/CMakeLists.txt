add_library(util
  child.cpp
  clock.cpp
  cgroups.cpp
  memfd.cpp
  output.cpp
  result.cpp
  stats.cpp
  system.cpp
)
target_link_libraries(util Folly::folly)

add_executable(stats_test stats_test.cpp)
target_link_libraries(stats_test util GTest::gtest GTest::gtest_main)
gtest_discover_tests(stats_test)

add_executable(system_tests system_test.cpp)
target_link_libraries(system_tests util GTest::gtest GTest::gtest_main)
gtest_discover_tests(system_tests)
