cmake_minimum_required(VERSION 3.0)

project(
    tsp_for_hpc
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(sources
    functions.cpp
    semaphore.cpp
    jitter.cpp
    run_cmd.cpp
    status_manager.cpp
    output_manager.cpp
    proc_manager.cpp
    tsp.cpp)

add_executable(tsp-hpc ${sources})

target_include_directories(tsp-hpc PUBLIC .)