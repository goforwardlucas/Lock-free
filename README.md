# Lock-free Queue Library

A header-only C++20 lock-free queue library implementing SPSC (Single-Producer Single-Consumer) and MPSC (Multi-Producer Single-Consumer) queues with verified correctness and benchmarked performance.

## Features

- **SPSC queue**: wait-free fast path, acquire/release memory ordering, no seq_cst fences
- **MPSC queue**: wait-free push via atomic exchange, Vyukov-style stub node design
- Cache-line isolation: `alignas(64)` separates producer/consumer indices, eliminating false sharing
- Verified with ThreadSanitizer: zero data races across 1M-item and 4-producer stress tests
- Benchmarked with Google Benchmark on i7-13700F

## Performance

| Benchmark | Throughput | vs mutex |
|---|---|---|
| SPSC (lock-free) | **288 M ops/s** | **21.3×** faster |
| mutex + std::queue | 13.5 M ops/s | baseline |

| Benchmark | Latency (round-trip) |
|---|---|
| SPSC cross-core ping-pong | **200 ns** |

### Cache-line verification
```
head_ cache line : N
tail_ cache line : N+1
gap              : 64 bytes
PASS: head and tail on separate cache lines
PASS: both atomics are 64-byte aligned
```

## Correctness

### Memory ordering rationale (SPSC)

| Operation | Memory order | Reason |
|---|---|---|
| `head_.load()` in push | `relaxed` | Only producer writes head — no cross-thread sync needed |
| `tail_.load()` in push | `acquire` | Must see consumer's latest tail write |
| `head_.store()` in push | `release` | Publishes data write to consumer |
| `tail_.load()` in pop | `relaxed` | Only consumer writes tail |
| `head_.load()` in pop | `acquire` | Pairs with producer's release — guarantees data visible |
| `tail_.store()` in pop | `release` | Notifies producer that slot is free |

### MPSC design

Vyukov-style intrusive linked list. `push` is wait-free via `tail_.exchange()`; `try_pop` is single-consumer only. A permanent stub node eliminates null-check edge cases on empty queue.

## Usage
```cpp
#include "spsc_queue.h"
#include "mpsc_queue.h"

// SPSC: capacity must be power of two
SPSCQueue<int, 4096> spsc;

// producer thread
spsc.try_push(42);

// consumer thread
auto val = spsc.try_pop(); // returns std::optional<int>

// MPSC: unbounded, multiple producers safe
MPSCQueue<int> mpsc;
mpsc.push(1);  // any thread
auto val2 = mpsc.try_pop(); // consumer thread only
```

## Build & Test
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Correctness tests
./build/test_spsc
./build/test_mpsc

# Cache-line layout verification
./build/test_layout

# Benchmarks (requires Google Benchmark)
./build/bench_queue
```

## Requirements

- C++20 compiler (g++ >= 10 or clang++ >= 12)
- CMake >= 3.20
- Google Benchmark (optional, for benchmarks)

## Environment

Benchmarks run on Intel i7-13700F, WSL2 Ubuntu 24.04, g++ 13.3.0, `-O3 -march=native`.
