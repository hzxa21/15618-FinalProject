cmake_minimum_required(VERSION 3.6)
project(huffcode)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
set(SOURCE_FILES huffcode.cpp)
add_library(huffman huffman.cpp)
add_executable(huffcode ${SOURCE_FILES})
target_link_libraries(huffcode huffman)