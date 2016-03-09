cmake_minimum_required(VERSION 2.8.9)
include(ExternalProject)

project(cs540p2 CXX)

find_package(Boost 1.49 REQUIRED COMPONENTS unit_test_framework)
include_directories(${Boost_INCLUDE_DIRS})

add_custom_target(map-header SOURCES Map.hpp)
add_library(map STATIC Map.cpp)
add_dependencies(map map-header)

add_executable(test-kec test-kec.cpp)
target_link_libraries(test-kec map)
add_executable(test test.cpp)
target_link_libraries(test map)
add_executable(minimal minimal.cpp)
target_link_libraries(minimal map)
add_executable(morseex morseex.cpp)
target_link_libraries(morseex map)
add_executable(test-scaling test-scaling.cpp)
target_link_libraries(test-scaling map)
add_custom_target(required)
add_dependencies(required test-kec test minimal morseex test-scaling)

add_executable(test-index test-index.cpp)
target_link_libraries(test-index map)
add_executable(test-index-complexity test-index-complexity.cpp)
target_link_libraries(test-index-complexity map pthread)
add_custom_target(extra)
add_dependencies(extra test-index)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -pedantic -Werror -Wfatal-errors")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -U NDEBUG")
