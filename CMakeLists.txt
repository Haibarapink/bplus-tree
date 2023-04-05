cmake_minimum_required(VERSION 3.22)
project(bplustree)
set(CMAKE_CXX_STANDARD 20)

# debug need DEBUG macro
add_compile_definitions(DEBUG)

# set compile flags -g
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

# set -g
set(CMAKE_CXX_FLAGS_RELEASE "-g")

add_executable(test_unit tests/test_unit.cc)
add_executable(test_bpt tests/test_bpt.cc)
add_executable(test_node_split tests/test_node_split.cc)
add_executable(test_node_store tests/test_node_store.cc)
add_executable(test_insert tests/test_insert.cc)