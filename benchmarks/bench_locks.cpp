// Task 1 benchmark: is_prime inside critical section + outside_work units outside.
// Usage: bench_locks --algorithm=<tas|ttas|ttas_backoff|mcs|std_mutex|pthread_mutex>
//        --threads=N --outside_work=K --iterations=I --reps=R --platform=P
// Emits one CSV row per rep.
#include "common.h"
#include "locks.h"

#include <thread>
#include <vector>

// Per-thread worker: `iterations` rounds; each round = outside_work unprotected
// prime units + 1 prime unit inside the lock. A shared scratch value inside the
// critical section forces real mutual exclusion (not just timing overlap).
template <typename Lock>
void worker(Lock& lock, long long& shared_scratch, int iterations,
            int outside_work, uint64_t seed) {
    uint64_t st = seed;
    for (int i = 0; i < iterations; i++) {
        for (int k = 0; k < outside_work; k++) do_prime_unit(st);
        lock.lock();
        do_prime_unit(st);
        shared_scratch++; // proves the CS is serialized; checked in tests
        lock.unlock();
    }
}

// MCS needs per-thread nodes — special-case it.
void worker_mcs(MCSLock& lock, long long& shared_scratch, int iterations,
                int outside_work, uint64_t seed) {
    MCSLock::Node me;
    uint64_t st = seed;
    for (int i = 0; i < iterations; i++) {
        for (int k = 0; k < outside_work; k++) do_prime_unit(st);
        lock.lock(me);
        do_prime_unit(st);
        shared_scratch++;
        lock.unlock(me);
    }
}

template <typename Lock>
double run_once(Lock& lock, int threads, int iterations, int outside_work,
                long long& scratch_out) {
    scratch_out = 0;
    std::vector<std::thread> ts;
    double t0 = now_seconds();
    for (int i = 0; i < threads; i++)
        ts.emplace_back(worker<Lock>, std::ref(lock), std::ref(scratch_out),
                        iterations, outside_work, 0x9e3779b97f4a7c15ULL + i);
    for (auto& t : ts) t.join();
    return now_seconds() - t0;
}

double run_once_mcs(MCSLock& lock, int threads, int iterations,
                    int outside_work, long long& scratch_out) {
    scratch_out = 0;
    std::vector<std::thread> ts;
    double t0 = now_seconds();
    for (int i = 0; i < threads; i++)
        ts.emplace_back(worker_mcs, std::ref(lock), std::ref(scratch_out),
                        iterations, outside_work, 0x9e3779b97f4a7c15ULL + i);
    for (auto& t : ts) t.join();
    return now_seconds() - t0;
}

int main(int argc, char** argv) {
    std::string alg = get_str_arg(argc, argv, "algorithm", "tas");
    int threads = get_int_arg(argc, argv, "threads", 4);
    int outside = get_int_arg(argc, argv, "outside_work", 0);
    int iters = get_int_arg(argc, argv, "iterations", 2000);
    int reps = get_int_arg(argc, argv, "reps", 1);
    std::string platform = get_str_arg(argc, argv, "platform", "unknown");

    for (int r = 0; r < reps; r++) {
        long long scratch = 0;
        double dt = 0;
        if (alg == "tas")            { TASLock l;          dt = run_once(l, threads, iters, outside, scratch); }
        else if (alg == "ttas")      { TTASLock l;         dt = run_once(l, threads, iters, outside, scratch); }
        else if (alg == "ttas_backoff") { TTASBackoffLock l; dt = run_once(l, threads, iters, outside, scratch); }
        else if (alg == "mcs")       { MCSLock l;          dt = run_once_mcs(l, threads, iters, outside, scratch); }
        else if (alg == "std_mutex") { StdMutexLock l;     dt = run_once(l, threads, iters, outside, scratch); }
#ifndef _WIN32
        else if (alg == "pthread_mutex") { PthreadLock l;  dt = run_once(l, threads, iters, outside, scratch); }
#endif
        else { fprintf(stderr, "unknown algorithm: %s\n", alg.c_str()); return 2; }

        csv_row(platform, 1, alg, threads, outside, "wall_seconds", dt, r);
        csv_row(platform, 1, alg, threads, outside, "critical_sections",
                (double)threads * iters, r);
        fprintf(stderr, "verify scratch=%lld expected=%lld %s\n", scratch,
                (long long)threads * iters,
                scratch == (long long)threads * iters ? "OK" : "MISMATCH");
    }
    return 0;
}
