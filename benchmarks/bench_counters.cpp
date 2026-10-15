// Task 3 benchmark: M total increments (iterations per thread) under contention.
// Usage: bench_counters --algorithm=<mutex|cas|cas_backoff|faa|mcs>
//        --threads=N --iterations=I --outside_work=K --reps=R --platform=P
#include "common.h"
#include "counters.h"
#include "locks.h"

#include <thread>
#include <vector>

template <typename Counter>
double run_once(Counter& c, int threads, int iterations, int outside_work,
                long long& final_out) {
    std::vector<std::thread> ts;
    double t0 = now_seconds();
    for (int i = 0; i < threads; i++)
        ts.emplace_back([&] {
            uint64_t st = 0x9e3779b97f4a7c15ULL + i;
            for (int k = 0; k < iterations; k++) {
                for (int w = 0; w < outside_work; w++) do_prime_unit(st);
                c.add(1);
            }
        });
    for (auto& t : ts) t.join();
    double dt = now_seconds() - t0;
    final_out = c.get();
    return dt;
}

int main(int argc, char** argv) {
    std::string alg = get_str_arg(argc, argv, "algorithm", "faa");
    int threads = get_int_arg(argc, argv, "threads", 4);
    int outside = get_int_arg(argc, argv, "outside_work", 0);
    int iters = get_int_arg(argc, argv, "iterations", 100000);
    int reps = get_int_arg(argc, argv, "reps", 1);
    std::string platform = get_str_arg(argc, argv, "platform", "unknown");

    for (int r = 0; r < reps; r++) {
        long long final = -1;
        double dt = 0;
        if (alg == "mutex")          { MutexCounter c;                  dt = run_once(c, threads, iters, outside, final); }
        else if (alg == "cas")       { CASCounter c;                    dt = run_once(c, threads, iters, outside, final); }
        else if (alg == "cas_backoff") { CASBackoffCounter c;           dt = run_once(c, threads, iters, outside, final); }
        else if (alg == "faa")       { FAACounter c;                    dt = run_once(c, threads, iters, outside, final); }
        else if (alg == "mcs") {
            // GenericLockCounter<MCSLock> cannot be used: MCS needs per-thread
            // nodes, incompatible with the flat add() API. Task 1 covers MCS.
            fprintf(stderr, "mcs not applicable for counter bench\n");
            return 2;
        }
        else { fprintf(stderr, "unknown algorithm: %s\n", alg.c_str()); return 2; }

        csv_row(platform, 3, alg, threads, outside, "wall_seconds", dt, r);
        csv_row(platform, 3, alg, threads, outside, "increments",
                (double)threads * iters, r);
        fprintf(stderr, "verify final=%lld expected=%lld %s\n", final,
                (long long)threads * iters,
                final == (long long)threads * iters ? "OK" : "MISMATCH");
    }
    return 0;
}
