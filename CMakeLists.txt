cmake_minimum_required(VERSION 3.14)
project(schtest)

set(CMAKE_CXX_STANDARD 20)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
enable_testing()
include(CTest)

include_directories(${CMAKE_SOURCE_DIR})
add_subdirectory(src)
