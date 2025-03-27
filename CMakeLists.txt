cmake_minimum_required(VERSION 3.0)

project(
    tsp_for_hpc
    LANGUAGES CXX)

find_package(SQLite3 REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

option(CGROUPV2 "Cgroup type used on target system" OFF)
if(CGROUPV2)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCGROUPV2")
endif(CGROUPV2)

set(sources
    parse_args.cpp
    locker.cpp
    functions.cpp
    jitter.cpp
    run_cmd.cpp
    sqlite_statement_manager.cpp
    status_manager.cpp
    status_writing.cpp
    output_manager.cpp
    proc_affinity.cpp
    spooler.cpp
    tsp.cpp)

add_executable(tsp-hpc ${sources})

target_link_libraries(tsp-hpc PUBLIC ${SQLite3_LIBRARIES})
include_directories(. ${SQLite3_INCLUDE_DIRS})