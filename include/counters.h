// Task 3: concurrent counters — mutex, CAS, CAS+backoff, fetch-and-add.
#pragma once

#include <atomic>
#include <mutex>
#include <thread>

class MutexCounter {
public:
    void add(long long d) { std::lock_guard<std::mutex> g(m_); v_ += d; }
    long long get() { std::lock_guard<std::mutex> g(m_); return v_; }
private:
    std::mutex m_;
    long long v_ = 0;
};

// Also allow plugging our own lock (e.g., MCS) as the counter's mutex.
template <typename Lock>
class GenericLockCounter {
public:
    void add(long long d) { l_.lock(); v_ += d; l_.unlock(); }
    long long get() { return v_; }
private:
    Lock l_;
    long long v_ = 0;
};

class CASCounter {
public:
    void add(long long d) {
        long long cur = v_.load(std::memory_order_relaxed);
        while (!v_.compare_exchange_weak(cur, cur + d,
                                         std::memory_order_acq_rel)) {
        }
    }
    long long get() { return v_.load(std::memory_order_acquire); }
private:
    std::atomic<long long> v_{0};
};

class CASBackoffCounter {
public:
    explicit CASBackoffCounter(int max_delay = 1 << 14) : max_delay_(max_delay) {}
    void add(long long d) {
        int delay = 1;
        long long cur = v_.load(std::memory_order_relaxed);
        while (!v_.compare_exchange_weak(cur, cur + d,
                                         std::memory_order_acq_rel)) {
            for (int i = 0; i < delay; i++) pause();
            if (delay < max_delay_) delay <<= 1;
        }
    }
    long long get() { return v_.load(std::memory_order_acquire); }
private:
    static void pause() {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#else
        std::this_thread::yield();
#endif
    }
    std::atomic<long long> v_{0};
    int max_delay_;
};

class FAACounter {
public:
    void add(long long d) { v_.fetch_add(d, std::memory_order_acq_rel); }
    long long get() { return v_.load(std::memory_order_acquire); }
private:
    std::atomic<long long> v_{0};
};
