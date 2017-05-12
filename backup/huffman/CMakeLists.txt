cmake_minimum_required(VERSION 3.7)
project(huffman)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp)
add_executable(huffman ${SOURCE_FILES})

set(ISPC_TEST ispc_test/main.cpp)
add_executable(ispc_test ${ISPC_TEST})