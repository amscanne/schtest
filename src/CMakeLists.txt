add_subdirectory(sched)

add_executable(schtest main.cpp)
target_link_libraries(schtest sched)
