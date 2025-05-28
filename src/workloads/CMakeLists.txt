add_library(workloads
  benchmark.cpp
  context.cpp
  process.cpp
  semaphore.cpp
  socketpair.cpp
  spinner.cpp
)
target_link_libraries(workloads util)
