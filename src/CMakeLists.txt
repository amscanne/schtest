add_subdirectory(sched)
add_subdirectory(util)
add_subdirectory(workloads)

add_executable(schtest
  main.cpp
  tests/basic.cpp
)
target_link_libraries(schtest
  sched
  util
  gtest
  workloads
)
