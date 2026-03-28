#include "spsc_queue.h"
#include <cstdio>
#include <atomic>

int main() {
    // alignas(64) 保证每个原子变量从 64 字节边界开始
    // 直接验证 atomic<size_t> 在 alignas(64) 下的对齐和间距
    alignas(64) std::atomic<std::size_t> head{0};
    alignas(64) std::atomic<std::size_t> tail{0};

    auto h = reinterpret_cast<uintptr_t>(&head);
    auto t = reinterpret_cast<uintptr_t>(&tail);
    std::ptrdiff_t gap = static_cast<std::ptrdiff_t>(t - h);

    std::printf("head address    : 0x%lx\n", h);
    std::printf("tail address    : 0x%lx\n", t);
    std::printf("head cache line : %lu\n", h / 64);
    std::printf("tail cache line : %lu\n", t / 64);
    std::printf("gap             : %td bytes\n", gap);

    if (h % 64 == 0 && t % 64 == 0)
        std::printf("PASS: both atomics are 64-byte aligned\n");
    else
        std::printf("FAIL: alignment wrong\n");

    if (gap >= 64)
        std::printf("PASS: head and tail on separate cache lines\n");
    else
        std::printf("FAIL: false sharing! gap=%td bytes\n", gap);

    return 0;
}
