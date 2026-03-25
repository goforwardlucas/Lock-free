//
// Created by Lucas on 25/03/2026.
//

#include "spsc_queue.h"
#include <thread>
#include <vector>
#include <numeric>
#include <cassert>
#include <cstdio>

// Test 1: basic single-threaded round-trip
void test_basic() {
    SPSCQueue<int, 8> q;
    assert(q.empty());
    assert(q.try_push(42));
    assert(!q.empty());
    auto v = q.try_pop();
    assert(v.has_value() && *v == 42);
    assert(q.empty());
    std::puts("PASS: basic");
}

// Test 2: fill to capacity, then drain
void test_full() {
    SPSCQueue<int, 4> q; // capacity 4, but ring uses one slot as sentinel
    // Power-of-2 ring: effective capacity is Capacity-1 = 3
    // (head==tail means empty; full when next==tail)
    assert(q.try_push(1));
    assert(q.try_push(2));
    assert(q.try_push(3));
    assert(!q.try_push(4)); // should be full (3 items fill a cap-4 ring)
    assert(*q.try_pop() == 1);
    assert(*q.try_pop() == 2);
    assert(*q.try_pop() == 3);
    assert(!q.try_pop().has_value());
    std::puts("PASS: full/drain");
}

// Test 3: multi-threaded, N items, verify ordering and no loss
void test_threaded(const std::size_t N = 1'000'000) {
    SPSCQueue<int, 1024> q;
    std::vector<int> results;
    results.reserve(N);

    std::thread producer([&] {
        for (int i = 0; i < static_cast<int>(N); ++i)
            while (!q.try_push(i)); // spin until space
    });

    for (std::size_t i = 0; i < N; ++i) {
        std::optional<int> v;
        while (!(v = q.try_pop())); // spin until item
        results.push_back(*v);
    }

    producer.join();

    // Verify: items must arrive in order and sum must match
    assert(results.size() == N);
    for (std::size_t i = 0; i < N; ++i)
        assert(results[i] == static_cast<int>(i));

    std::puts("PASS: threaded ordering (1M items)");
}

int main() {
    test_basic();
    test_full();
    test_threaded();
    std::puts("All test passed.");
}