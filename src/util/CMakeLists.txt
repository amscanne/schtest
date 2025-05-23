add_library(util
  clock.cpp
  result.cpp
)
target_link_libraries(util Folly::folly)
