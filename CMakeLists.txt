cmake_minimum_required(VERSION 3.0)
set(HWLOC_MIN_VERSION 2.5.0)
set(SQLITE_MIN_VERSION 3.14)

project(
    tsp_for_hpc
    LANGUAGES CXX)

include(${CMAKE_CURRENT_SOURCE_DIR}/Findhwloc.cmake)
if (hwloc_VERSION VERSION_LESS ${HWLOC_MIN_VERSION})
    message(FATAL_ERROR "hwloc version ${hwloc_VERSION} is less than the minimum required: ${HWLOC_MIN_VERSION}")
endif ()

find_package(SQLite3 ${SQLITE_MIN_VERSION} REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(sources
    locker.cpp
    functions.cpp
    generic_config.cpp
    jitter.cpp
    run_cmd.cpp
    sqlite_statement_manager.cpp
    status_manager.cpp
    status_writing.cpp
    output_manager.cpp
    proc_affinity.cpp
    spooler.cpp
    timeout.cpp)

### Windows compatibility is a pipedream
IF (NOT APPLE AND NOT WIN32)
    list(APPEND sources linux_proc_tools.cpp memprof_manager.cpp memprof.cpp)
endif ()

### Purely aesthetic
list(APPEND sources tsp.cpp)

add_executable(tsp-hpc ${sources})

target_link_libraries(tsp-hpc PUBLIC ${SQLite3_LIBRARIES} ${hwloc_LIBRARIES})
include_directories(. ${SQLite3_INCLUDE_DIRS} ${hwloc_INCLUDE_DIRS})