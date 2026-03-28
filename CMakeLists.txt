cmake_minimum_required(VERSION 3.20)
project(lockfree_queue CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-O3 -march=native -fno-omit-frame-pointer -g)

# Header-only library
add_library(lockfree_queue INTERFACE)
target_include_directories(lockfree_queue INTERFACE include/)

# Tests
add_executable(test_spsc test/test_spsc.cpp)
target_link_libraries(test_spsc PRIVATE lockfree_queue pthread)

# Benchmarks
find_package(benchmark REQUIRED)
add_executable(bench_queue bench/bench_queue.cpp)
target_link_libraries(bench_queue PRIVATE lockfree_queue benchmark::benchmark pthread)

add_executable(test_layout test/test_layout.cpp)
target_link_libraries(test_layout PRIVATE lockfree_queue)

add_executable(test_mpsc test/test_mpsc.cpp)
target_link_libraries(test_mpsc PRIVATE lockfree_queue pthread)
