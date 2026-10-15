cmake_minimum_required(VERSION 3.16)
project(parallel_assignment1 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include_directories(include)

if(WIN32)
    # MSVC: no POSIX threads; pthread_* targets are compile-guarded out.
    add_compile_options(/O2)
else()
    add_compile_options(-O2)
    set(CMAKE_THREAD_LIBS_INIT "-pthread")
endif()

set(COMMON_LINK ${CMAKE_THREAD_LIBS_INIT})

add_executable(test_locks    tests/test_locks.cpp)
add_executable(test_barrier  tests/test_barrier.cpp)
add_executable(test_counters tests/test_counters.cpp)
add_executable(bench_locks    benchmarks/bench_locks.cpp)
add_executable(bench_barrier  benchmarks/bench_barrier.cpp)
add_executable(bench_counters benchmarks/bench_counters.cpp)
foreach(t test_locks test_barrier test_counters bench_locks bench_barrier bench_counters)
    target_link_libraries(${t} ${COMMON_LINK})
endforeach()

# Task 4: MPI-3 one-sided — optional; skipped when MPI is unavailable so the
# shared-memory tasks build everywhere without an MPI install.
find_package(MPI QUIET COMPONENTS CXX)
if(MPI_CXX_FOUND)
    add_executable(bench_mpi src/distributed/bench_mpi.cpp)
    add_executable(test_mpi tests/test_mpi.cpp)
    target_link_libraries(bench_mpi MPI::MPI_CXX)
    target_link_libraries(test_mpi MPI::MPI_CXX)
    message(STATUS "MPI found: building bench_mpi, test_mpi")
else()
    message(STATUS "MPI not found: bench_mpi/test_mpi (Task 4) skipped")
endif()
