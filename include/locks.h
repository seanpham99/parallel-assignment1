// Task 1: mutex lock implementations — TAS, TTAS, TTAS+exp-backoff, MCS.
// Plus adapters for std::mutex and (Unix) pthread_mutex_t baselines.
#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#ifndef _WIN32
#include <pthread.h>
#endif

// ---------------- TAS ----------------
class TASLock {
public:
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
        }
    }
    void unlock() { flag.clear(std::memory_order_release); }

private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

// ---------------- TTAS ----------------
class TTASLock {
public:
    void lock() {
        while (true) {
            // Spin reading until the lock looks free (reduces coherency traffic).
            while (flag.test(std::memory_order_relaxed)) {
            }
            if (!flag.test_and_set(std::memory_order_acquire)) return;
        }
    }
    void unlock() { flag.clear(std::memory_order_release); }

private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

// ---------------- TTAS + exponential backoff ----------------
class TTASBackoffLock {
public:
    explicit TTASBackoffLock(int max_delay = 1 << 16) : max_delay_(max_delay < 1 ? 1 : max_delay) {}

    void lock() {
        int delay = 1;
        while (true) {
            while (flag.test(std::memory_order_relaxed)) {
            }
            if (!flag.test_and_set(std::memory_order_acquire)) return;
            // Failed CAS: back off exponentially, bounded.
            for (int i = 0; i < delay; i++) pause();
            if (delay < max_delay_) delay <<= 1;
        }
    }
    void unlock() { flag.clear(std::memory_order_release); }

private:
    static void pause() {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#else
        std::this_thread::yield();
#endif
    }
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    int max_delay_;
};

// ---------------- MCS ----------------
// Queue-based spinlock: each thread spins on its OWN node, so spin traffic is
// a single cache line per waiter instead of all threads hammering one word.
class MCSLock {
public:
    struct Node {
        std::atomic<bool> locked{false};
        std::atomic<Node*> next{nullptr};
    };

    void lock(Node& me) {
        me.next.store(nullptr, std::memory_order_relaxed);
        Node* pred = tail.exchange(&me, std::memory_order_acq_rel);
        if (pred) {
            me.locked.store(true, std::memory_order_relaxed);
            pred->next.store(&me, std::memory_order_release);
            while (me.locked.load(std::memory_order_acquire)) {
            }
        }
    }

    void unlock(Node& me) {
        if (!me.next.load(std::memory_order_acquire)) {
            Node* expected = &me;
            if (tail.compare_exchange_strong(expected, nullptr,
                                             std::memory_order_acq_rel)) {
                return;
            }
            // Successor is arriving; wait until it publishes its next pointer.
            while (!me.next.load(std::memory_order_acquire)) {
            }
        }
        me.next.load(std::memory_order_relaxed)->locked.store(
            false, std::memory_order_release);
    }

private:
    std::atomic<Node*> tail{nullptr};
};

// ---------------- baseline adapters ----------------
class StdMutexLock {
public:
    void lock() { m_.lock(); }
    void unlock() { m_.unlock(); }
private:
    std::mutex m_;
};

#ifndef _WIN32
class PthreadLock {
public:
    PthreadLock() { pthread_mutex_init(&m_, nullptr); }
    ~PthreadLock() { pthread_mutex_destroy(&m_); }
    void lock() { pthread_mutex_lock(&m_); }
    void unlock() { pthread_mutex_unlock(&m_); }
private:
    pthread_mutex_t m_;
};
#endif
