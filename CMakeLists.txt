cmake_minimum_required(VERSION 3.14)
project(schtest)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

set(CMAKE_CXX_STANDARD 20)
set(BUILD_SHARED_LIBS OFF)
set(FOLLY_USE_JEMALLOC OFF)

find_package(GFlags REQUIRED)
find_package(GTest REQUIRED)
find_package(Folly REQUIRED)
enable_testing()
include(CTest)

add_compile_definitions(-DGLOG_USE_GLOG_EXPORT)

include_directories(${CMAKE_SOURCE_DIR}/src)
add_subdirectory(src)
