
#include "spsc_queue.h"
#include <benchmark/benchmark.h>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <pthread.h>
#include <sched.h>

// 把当前线程绑到指定 CPU
static void pin_to_cpu(int cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
}

// ── SPSC 吞吐量（consumer=cpu0, producer=cpu1）────────────────
static void BM_SPSC_Throughput(benchmark::State& state) {
    SPSCQueue<int, 65536> q;
    std::atomic<bool> running{true};
    std::atomic<bool> ready{false};

    pin_to_cpu(0); // consumer 绑 cpu0

    std::thread producer([&] {
        pin_to_cpu(1); // producer 绑 cpu1
        int i = 0;
        ready.store(true, std::memory_order_release);
        while (running.load(std::memory_order_relaxed))
            q.try_push(i++);
    });

    while (!ready.load(std::memory_order_acquire));

    for (auto _ : state) {
        while (!q.try_pop());
    }

    running.store(false);
    producer.join();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSC_Throughput)->UseRealTime()->MinTime(2.0);

// ── mutex 基准线（同样跨核）───────────────────────────────────
static void BM_Mutex_Throughput(benchmark::State& state) {
    std::queue<int> q;
    std::mutex mu;
    std::atomic<bool> running{true};
    std::atomic<bool> ready{false};

    pin_to_cpu(0);

    std::thread producer([&] {
        pin_to_cpu(1);
        int i = 0;
        ready.store(true, std::memory_order_release);
        while (running.load(std::memory_order_relaxed)) {
            std::lock_guard<std::mutex> lk(mu);
            q.push(i++);
        }
    });

    while (!ready.load(std::memory_order_acquire));

    for (auto _ : state) {
        bool got = false;
        while (!got) {
            std::lock_guard<std::mutex> lk(mu);
            if (!q.empty()) { q.pop(); got = true; }
        }
    }

    running.store(false);
    producer.join();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Mutex_Throughput)->UseRealTime()->MinTime(2.0);

// ── SPSC 延迟 ping-pong（跨核往返）───────────────────────────
static void BM_SPSC_Latency(benchmark::State& state) {
    SPSCQueue<int, 4> ping;
    SPSCQueue<int, 4> pong;
    std::atomic<bool> running{true};

    pin_to_cpu(0);

    std::thread partner([&] {
        pin_to_cpu(1);
        while (running.load(std::memory_order_relaxed)) {
            if (ping.try_pop())
                while (!pong.try_push(1) &&
                       running.load(std::memory_order_relaxed));
        }
    });

    while (!ping.try_push(1));
    while (!pong.try_pop());

    for (auto _ : state) {
        while (!ping.try_push(1));
        while (!pong.try_pop());
    }

    running.store(false);
    partner.join();
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSC_Latency)->UseRealTime()->MinTime(2.0);

BENCHMARK_MAIN();
