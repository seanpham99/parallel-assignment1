// Task 1 benchmark: is_prime inside critical section + outside_work units outside.
// Usage: bench_locks --algorithm=<tas|ttas|ttas_backoff|mcs|std_mutex|pthread_mutex>
//        --threads=N --outside_work=K --iterations=I --reps=R --platform=P
// Emits one CSV row per rep.
#include "common.h"
#include "locks.h"

#include <thread>
#include <vector>


// Workers stop early if the rep exceeds the time budget — oversubscribed
// threads on few cores convoy badly, and unbounded runs stall the sweep for
// hours. The CSV row reports the actual completed critical_sections.
static std::atomic<bool> g_stop{false};

template <typename Lock>
void worker(Lock& lock, std::atomic<long long>& cs_done, int iterations,
            int outside_work, uint64_t seed) {
    uint64_t st = seed;
    for (int i = 0; i < iterations && !g_stop.load(std::memory_order_relaxed);
         i++) {
        for (int k = 0; k < outside_work; k++) do_prime_unit(st);
        lock.lock();
        do_prime_unit(st);
        lock.unlock();
        cs_done.fetch_add(1, std::memory_order_relaxed);
    }
}

void worker_mcs(MCSLock& lock, std::atomic<long long>& cs_done, int iterations,
                int outside_work, uint64_t seed) {
    MCSLock::Node me;
    uint64_t st = seed;
    for (int i = 0; i < iterations && !g_stop.load(std::memory_order_relaxed);
         i++) {
        for (int k = 0; k < outside_work; k++) do_prime_unit(st);
        lock.lock(me);
        do_prime_unit(st);
        lock.unlock(me);
        cs_done.fetch_add(1, std::memory_order_relaxed);
    }
}

template <typename Lock>
double run_once(Lock& lock, int threads, int iterations, int outside_work,
                double budget_s, long long& cs_done_out) {
    std::atomic<long long> cs_done{0};
    g_stop.store(false, std::memory_order_relaxed);
    std::vector<std::thread> ts;
    double t0 = now_seconds();
    for (int i = 0; i < threads; i++)
        ts.emplace_back(worker<Lock>, std::ref(lock), std::ref(cs_done),
                        iterations, outside_work, 0x9e3779b97f4a7c15ULL + i);
    std::thread watcher([&] {
        while (now_seconds() - t0 < budget_s &&
               !g_stop.load(std::memory_order_relaxed))
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        g_stop.store(true, std::memory_order_relaxed);
    });
    for (auto& t : ts) t.join();
    double dt = now_seconds() - t0;
    g_stop.store(true); // make watcher exit
    watcher.join();
    cs_done_out = cs_done.load();
    return dt;
}

double run_once_mcs(MCSLock& lock, int threads, int iterations,
                    int outside_work, double budget_s, long long& cs_done_out) {
    std::atomic<long long> cs_done{0};
    g_stop.store(false, std::memory_order_relaxed);
    std::vector<std::thread> ts;
    double t0 = now_seconds();
    for (int i = 0; i < threads; i++)
        ts.emplace_back(worker_mcs, std::ref(lock), std::ref(cs_done),
                        iterations, outside_work, 0x9e3779b97f4a7c15ULL + i);
    std::thread watcher([&] {
        while (now_seconds() - t0 < budget_s &&
               !g_stop.load(std::memory_order_relaxed))
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        g_stop.store(true, std::memory_order_relaxed);
    });
    for (auto& t : ts) t.join();
    double dt = now_seconds() - t0;
    g_stop.store(true);
    watcher.join();
    cs_done_out = cs_done.load();
    return dt;
}

int main(int argc, char** argv) {
    std::string alg = get_str_arg(argc, argv, "algorithm", "tas");
    int threads = get_int_arg(argc, argv, "threads", 4);
    int outside = get_int_arg(argc, argv, "outside_work", 0);
    int iters = get_int_arg(argc, argv, "iterations", 2000);
    int reps = get_int_arg(argc, argv, "reps", 1);
    double budget = get_int_arg(argc, argv, "budget", 30);
    std::string platform = get_str_arg(argc, argv, "platform", "unknown");

    for (int r = 0; r < reps; r++) {
        long long done = 0;
        double dt = 0;
        if (alg == "tas")            { TASLock l;          dt = run_once(l, threads, iters, outside, budget, done); }
        else if (alg == "ttas")      { TTASLock l;         dt = run_once(l, threads, iters, outside, budget, done); }
        else if (alg == "ttas_backoff") { TTASBackoffLock l; dt = run_once(l, threads, iters, outside, budget, done); }
        else if (alg == "mcs")       { MCSLock l;          dt = run_once_mcs(l, threads, iters, outside, budget, done); }
        else if (alg == "std_mutex") { StdMutexLock l;     dt = run_once(l, threads, iters, outside, budget, done); }
#ifndef _WIN32
        else if (alg == "pthread_mutex") { PthreadLock l;  dt = run_once(l, threads, iters, outside, budget, done); }
#endif
        else { fprintf(stderr, "unknown algorithm: %s\n", alg.c_str()); return 2; }

        csv_row(platform, 1, alg, threads, outside, "wall_seconds", dt, r);
        csv_row(platform, 1, alg, threads, outside, "critical_sections",
                (double)done, r);
        csv_row(platform, 1, alg, threads, outside, "cs_per_sec",
                done / dt, r);
        fprintf(stderr, "verify done=%lld target=%lld budget=%.0fs %s\n", done,
                (long long)threads * iters, budget,
                done == (long long)threads * iters ? "OK" : "BUDGET-CUT");
    }
    return 0;
}
