cmake_minimum_required(VERSION 3.14)
project(schtest)
cmake_policy(SET CMP0167 OLD)

set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)

find_package(GLog REQUIRED)
find_package(GFlags REQUIRED)
find_package(GTest REQUIRED)
find_package(Folly REQUIRED)
enable_testing()
include(CTest)

add_compile_definitions(-DGLOG_USE_GLOG_EXPORT)

include_directories(${CMAKE_SOURCE_DIR}/src)
add_subdirectory(src)
