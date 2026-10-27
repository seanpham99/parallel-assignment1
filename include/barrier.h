// Task 2: Sense-Reversing Barrier.
// Centralized barrier: last arriver flips the shared sense; waiters spin on the
// flipped value. Safe for reuse because each round's sense is the complement of
// the previous round's.
#pragma once

#include <atomic>
#include <thread>

#ifndef _WIN32
#include <pthread.h>
#endif

class SenseBarrier {
public:
    explicit SenseBarrier(int n) : n_(n), count_(0), sense_(false), exits_(0) {}

    // Canonical sense-reversing barrier: caller keeps a thread-local sense bool.
    // Convention: sense_ starts false, so callers initialize their sense to true
    // and flip it after each arrive() call.
    // exits_ prevents the last arriver from returning (and re-entering the
    // next round) until every other thread has left this round, which closes
    // the re-entry race on count_.
    void arrive(bool thread_sense) {
        if (count_.fetch_add(1, std::memory_order_acq_rel) == n_ - 1) {
            count_.store(0, std::memory_order_release);
            sense_.store(thread_sense, std::memory_order_release);
            // Wait until all other threads have exited this round.
            while (exits_.load(std::memory_order_acquire) != n_ - 1) {
            }
            exits_.store(0, std::memory_order_relaxed);
        } else {
            while (sense_.load(std::memory_order_acquire) != thread_sense) {
            }
            exits_.fetch_add(1, std::memory_order_release);
        }
    }

private:
    int n_;
    std::atomic<int> count_;
    std::atomic<bool> sense_;
    std::atomic<int> exits_;
};

// Same barrier with exponential backoff in the spin loop (report variant).
class SenseBarrierBackoff {
public:
    explicit SenseBarrierBackoff(int n) : n_(n), count_(0), sense_(false),
                                         exits_(0) {}

    void arrive(bool thread_sense) {
        if (count_.fetch_add(1, std::memory_order_acq_rel) == n_ - 1) {
            count_.store(0, std::memory_order_release);
            sense_.store(thread_sense, std::memory_order_release);
            int delay = 1;
            while (exits_.load(std::memory_order_acquire) != n_ - 1) {
                for (int i = 0; i < delay; i++) pause();
                if (delay < (1 << 16)) delay <<= 1;
            }
            exits_.store(0, std::memory_order_relaxed);
        } else {
            int delay = 1;
            while (sense_.load(std::memory_order_acquire) != thread_sense) {
                for (int i = 0; i < delay; i++) pause();
                if (delay < (1 << 16)) delay <<= 1;
            }
            exits_.fetch_add(1, std::memory_order_release);
        }
    }

private:
    static void pause() {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#else
        std::this_thread::yield();
#endif
    }
    int n_;
    std::atomic<int> count_{0};
    std::atomic<bool> sense_{false};
    std::atomic<int> exits_{0};
};

// pthread_barrier_t: NOT available on macOS (Apple never implemented the
// optional POSIX barrier API). Guarded out there; Linux keeps it, Windows
// lacks pthreads entirely.
#if !defined(_WIN32) && !defined(__APPLE__)
#include <pthread.h>
class PthreadBarrier {
public:
    explicit PthreadBarrier(int n) { pthread_barrier_init(&b_, nullptr, n); }
    ~PthreadBarrier() { pthread_barrier_destroy(&b_); }
    void arrive() { pthread_barrier_wait(&b_); }
private:
    pthread_barrier_t b_;
};
#define HAVE_PTHREAD_BARRIER 1
#endif

#include <barrier>
class StdBarrier {
public:
    explicit StdBarrier(int n) : b_(n) {}
    void arrive() { b_.arrive_and_wait(); }
private:
    std::barrier<> b_;
};
