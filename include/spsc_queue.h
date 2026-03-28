//
// Created by Lucas on 25/03/2026.
//

#pragma once
#include <atomic>
#include <array>
#include <optional>
#include <cassert>
#include <cstddef>
#include <type_traits>

/// Single-Producer Single-Consumer bounded ring-buffer queue.
///
/// Guarantees:
///   - Wait-free for both producer and consumer (no loops on the fast path)
///   - Correct under C++20 memory model via acquire/release pairing
///   - No false sharing between producer and consumer indices
///
/// Constraints:
///   - Capacity must be a power of two (asserted at compile time)
///   - T must be move-constructible
///   - try_push called ONLY from one thread; try_pop from ONE other thread



///   The parameters are provided as template arguments, not constructor arguments, so they are known at compile time. This enables optimizations like static assertions, fixed-size buffers, and efficient indexing.
template <typename T, std::size_t Capacity>
class SPSCQueue {
    static_assert(Capacity >= 2 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two >= 2");
    static_assert(std::is_move_constructible_v<T>);

  public:
    //explicitly requests the compiler-generated constructor, preserving triviality and enabling better optimizations compared to a manually defined empty constructor.
    SPSCQueue() = default;

    // Non-copyable, non-movable (atomics are not movable)
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    /// Push one item. Returns false if queue is full (non-blocking).
    /// Call ONLY from the producer thread.
    [[nodiscard]] bool try_push(T val) noexcept(std::is_nothrow_move_assignable_v<T>) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        //                        ^^^^^^^ We are the only writer of head_;
        //                                reading our own last-written value needs no sync.

        const std::size_t next = (head + 1) & kMask; //modulo for wrap-around, valid because Capacity is a power of two.

        if (next == tail_.load(std::memory_order_acquire))
            //              ^^^^^^^ acquire: pairs with consumer's release-store to tail_.
            //              Ensures we see the consumer's latest slot-free notification.
            return false; // full

        data_[head] = std::move(val);
        //  ^^^^^^^^ Plain assignment. The release-store below acts as the publication
        //           fence — the compiler/CPU cannot reorder this store past the
        //           release-store on head_.

        head_.store(next, std::memory_order_release);
        //                ^^^^^^^ release: "all writes above are visible to any thread
        //                that subsequently acquire-loads head_."
        return true;
    }

    /// Pop one item. Returns nullopt if queue is empty (non-blocking).
    /// Call ONLY from the consumer thread.
    [[nodiscard]] std::optional<T> try_pop() noexcept(std::is_nothrow_move_constructible_v<T>) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        //                        ^^^^^^^ Same reasoning: only we write tail_.

        if (tail == head_.load(std::memory_order_acquire))
            //              ^^^^^^^ acquire: pairs with producer's release-store to head_.
            //              Guarantees data_[tail] write is visible before we read it.
            return std::nullopt; // empty

        T val = std::move(data_[tail]);

        tail_.store((tail + 1) & kMask, std::memory_order_release);
        //                              ^^^^^^^ release: tells producer this slot is free.
        return val;
    }

    /// Approximate size. May be stale by the time you use it.
    [[nodiscard]] std::size_t size_approx() const noexcept {
        const std::size_t h = head_.load(std::memory_order_acquire);
        const std::size_t t = tail_.load(std::memory_order_acquire);
        return (h - t + Capacity) & kMask;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    static constexpr std::size_t kMask = Capacity - 1;

    // ┌─────────────────────────────────────────────────────┐
    // │  Cache-line layout (64 bytes each on x86-64)        │
    // │                                                     │
    // │  [head_  | 56 bytes padding]  ← producer's line    │
    // │  [tail_  | 56 bytes padding]  ← consumer's line     │
    // │  [data_  ...]                 ← shared, read-only   │
    // │                               structure after init  │
    // └─────────────────────────────────────────────────────┘
#ifdef SPSC_NO_PADDING
    // Intentionally bad: head_ and tail_ share one cache line
    // Used ONLY for false-sharing benchmark
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
#else
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
#endif

    // data_ is placed after both atomics so it doesn't share their cache lines.
    // Individual slots are accessed by only one side at a time (ring invariant),
    // so there is no false sharing within data_ either.
    std::array<T, Capacity> data_;
};
