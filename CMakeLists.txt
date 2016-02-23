cmake_minimum_required(VERSION 2.8.9)
include(ExternalProject)

project(cs540p2 CXX)

find_package(Boost 1.49 REQUIRED COMPONENTS unit_test_framework)
include_directories(${Boost_INCLUDE_DIRS})

add_custom_target(map SOURCES Map.hpp)

add_executable(test-kec test-kec.cpp)
add_dependencies(test-kec map)
add_executable(test test.cpp)
add_dependencies(test map)
add_executable(minimal minimal.cpp)
add_dependencies(minimal map)
add_executable(morseex morseex.cpp)
add_dependencies(morseex map)
add_executable(test-scaling test-scaling.cpp)
add_dependencies(test-scaling map)
add_custom_target(required)
add_dependencies(required test-kec test minimal morseex test-scaling)

add_executable(test-index test-index.cpp)
add_dependencies(test-index map)
add_executable(test-index-complexity test-index-complexity.cpp)
add_dependencies(test-index map)
add_custom_target(extra)
add_dependencies(extra test-index)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -pedantic -Werror -Wfatal-errors -Wno-unused-but-set-variable")
