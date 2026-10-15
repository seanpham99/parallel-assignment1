// Task 3 benchmark: M total increments (iterations per thread) under contention.
// Usage: bench_counters --algorithm=<mutex|cas|cas_backoff|faa|mcs>
//        --threads=N --iterations=I --outside_work=K --reps=R --platform=P
#include "common.h"
#include "counters.h"
#include "locks.h"

#include <thread>
#include <vector>

// Same time-budget scheme as bench_locks: workers poll g_stop; holders always
// progress (add() is lock-free/bounded) so early stop cannot deadlock.
static std::atomic<bool> g_stop{false};

template <typename Counter>
double run_once(Counter& c, int threads, int iterations, int outside_work,
                double budget_s, long long& ops_done_out) {
    std::atomic<long long> ops_done{0};
    g_stop.store(false, std::memory_order_relaxed);
    std::vector<std::thread> ts;
    double t0 = now_seconds();
    for (int i = 0; i < threads; i++)
        ts.emplace_back([&] {
            uint64_t st = 0x9e3779b97f4a7c15ULL + i;
            for (int k = 0; k < iterations &&
                        !g_stop.load(std::memory_order_relaxed); k++) {
                for (int w = 0; w < outside_work; w++) do_prime_unit(st);
                c.add(1);
                ops_done.fetch_add(1, std::memory_order_relaxed);
            }
        });
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
    ops_done_out = ops_done.load();
    return dt;
}

int main(int argc, char** argv) {
    std::string alg = get_str_arg(argc, argv, "algorithm", "faa");
    int threads = get_int_arg(argc, argv, "threads", 4);
    int outside = get_int_arg(argc, argv, "outside_work", 0);
    int iters = get_int_arg(argc, argv, "iterations", 100000);
    int reps = get_int_arg(argc, argv, "reps", 1);
    std::string platform = get_str_arg(argc, argv, "platform", "unknown");
    double budget = get_int_arg(argc, argv, "budget", 30);

    for (int r = 0; r < reps; r++) {
        long long done = 0;
        double dt = 0;
        if (alg == "mutex")          { MutexCounter c;                  dt = run_once(c, threads, iters, outside, budget, done); }
        else if (alg == "cas")       { CASCounter c;                    dt = run_once(c, threads, iters, outside, budget, done); }
        else if (alg == "cas_backoff") { CASBackoffCounter c;           dt = run_once(c, threads, iters, outside, budget, done); }
        else if (alg == "faa")       { FAACounter c;                    dt = run_once(c, threads, iters, outside, budget, done); }
        else if (alg == "mcs") {
            // GenericLockCounter<MCSLock> cannot be used: MCS needs per-thread
            // nodes, incompatible with the flat add() API. Task 1 covers MCS.
            fprintf(stderr, "mcs not applicable for counter bench\n");
            return 2;
        }
        else { fprintf(stderr, "unknown algorithm: %s\n", alg.c_str()); return 2; }

        csv_row(platform, 3, alg, threads, outside, "wall_seconds", dt, r);
        csv_row(platform, 3, alg, threads, outside, "increments",
                (double)done, r);
        csv_row(platform, 3, alg, threads, outside, "ops_per_sec",
                done / dt, r);
        fprintf(stderr, "verify done=%lld target=%lld %s\n", done,
                (long long)threads * iters,
                done == (long long)threads * iters ? "OK" : "BUDGET-CUT");
    }
    return 0;
}
