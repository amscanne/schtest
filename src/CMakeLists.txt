add_subdirectory(sched)
add_subdirectory(util)
add_subdirectory(workloads)

add_executable(schtest
  main.cpp
  tests/basic.cpp
  tests/hyperthreads.cpp
)
target_link_libraries(schtest
  sched
  util
  GTest::gtest
  workloads
)
install(TARGETS schtest DESTINATION bin)
