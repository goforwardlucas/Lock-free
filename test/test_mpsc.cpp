#include "mpsc_queue.h"
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
#include <cstdio>

// Test 1: 基本单线程
void test_basic() {
    MPSCQueue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);
    assert(*q.try_pop() == 1);
    assert(*q.try_pop() == 2);
    assert(*q.try_pop() == 3);
    assert(!q.try_pop().has_value());
    std::puts("PASS: basic");
}

// Test 2: 4 个 producer，1 个 consumer，验证无丢失
void test_mpsc(const int producers = 4, const int per = 100000) {
    MPSCQueue<int> q;
    std::atomic<int> total_pushed{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < producers; ++i) {
        threads.emplace_back([&, i] {
            for (int j = 0; j < per; ++j)
                q.push(i * per + j);
            total_pushed.fetch_add(per, std::memory_order_relaxed);
        });
    }

    int consumed = 0;
    while (consumed < producers * per) {
        if (q.try_pop()) ++consumed;
    }

    for (auto& t : threads) t.join();
    assert(consumed == producers * per);
    std::puts("PASS: 4 producers x 100k items, no loss");
}

int main() {
    test_basic();
    test_mpsc();
    std::puts("All MPSC tests passed.");
}