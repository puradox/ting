cmake_minimum_required(VERSION 2.8.5)

# Project details
project(ting)
include_directories(include)

# Compiler configuration
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -g")

# Enable testing
enable_testing()
add_subdirectory(test)
