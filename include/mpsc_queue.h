


#pragma once
#include <atomic>
#include <optional>
#include <memory>

template <typename T>
class MPSCQueue {
public:
    MPSCQueue() {
        // stub node: 队列永远有一个哑节点
        // pop 从 stub->next 开始消费，避免 head/tail 为 null 的边界情况
        Node* stub = new Node{};
        head_ = stub;
        tail_.store(stub, std::memory_order_relaxed);
    }

    ~MPSCQueue() {
        while (try_pop()) {}
        delete head_; // 删除最后的 stub
    }

    // 任意线程可调用，wait-free
    void push(T val) {
        Node* node = new Node{std::move(val)};
        // 原子地把 node 接到链表尾部
        // exchange 返回旧 tail，然后旧 tail->next = node
        Node* prev = tail_.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    // 只能从单一 consumer 线程调用
    std::optional<T> try_pop() {
        Node* head = head_;
        Node* next = head->next.load(std::memory_order_acquire);
        if (!next) return std::nullopt; // 队列为空
        // next 成为新的 stub，把值取出
        head_ = next;
        T val = std::move(next->val);
        delete head; // 删除旧 stub
        return val;
    }

private:
    struct Node {
        T val{};
        std::atomic<Node*> next{nullptr};
    };

    // head_ 只有 consumer 访问，无需 atomic
    Node* head_;
    // tail_ 多个 producer 竞争，需要 atomic
    alignas(64) std::atomic<Node*> tail_;
};
