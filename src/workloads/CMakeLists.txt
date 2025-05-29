add_library(workloads
  benchmark.cpp
  context.cpp
  process.cpp
  semaphore.cpp
  spinner.cpp
)
target_link_libraries(workloads gflags util)
