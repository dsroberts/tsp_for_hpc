cmake_minimum_required(VERSION 3.0)

project(tsp_for_hpc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

add_executable(tsp-hpc tsp.cpp)