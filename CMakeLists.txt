cmake_minimum_required(VERSION 3.14)
project(schtest)

set(CMAKE_CXX_STANDARD 20)

find_package(GTest REQUIRED)
find_package(Folly REQUIRED)
find_package(GFlags REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})
enable_testing()
include(CTest)

include_directories(${CMAKE_SOURCE_DIR}/src)
add_subdirectory(src)
