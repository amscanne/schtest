add_library(util
  clock.cpp
  cgroups.cpp
  result.cpp
  stats.cpp
  system.cpp
)
target_link_libraries(util Folly::folly)
