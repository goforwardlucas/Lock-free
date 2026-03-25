cmake_minimum_required(VERSION 3.20)
project(lockfree_queue CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compile flags (tune for your machine — znver3 for Zen 3, native for local)
add_compile_options(-O3 -march=native -fno-omit-frame-pointer -g)

# Header-only library target
add_library(lockfree_queue INTERFACE)
target_include_directories(lockfree_queue INTERFACE include/)

# Tests
add_executable(test_spsc test/test_spsc.cpp)
target_link_libraries(test_spsc PRIVATE lockfree_queue)

# Benchmarks (requires Google Benchmark installed)
find_package(benchmark QUIET)
if(benchmark_FOUND)
    add_executable(bench_queue bench/bench_queue.cpp)
    target_link_libraries(bench_queue PRIVATE lockfree_queue benchmark::benchmark)
endif()