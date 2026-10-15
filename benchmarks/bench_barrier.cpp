// Task 2 benchmark: barrier episodes per second, zero useful work between.
// Usage: bench_barrier --algorithm=<srb|srb_backoff|pthread_barrier|std_barrier>
//        --threads=N --episodes=E --reps=R --platform=P
#include "common.h"
#include "barrier.h"

#include <thread>
#include <vector>

template <typename Barrier>
double run_once(Barrier& bar, int threads, int episodes) {
    std::vector<std::thread> ts;
    double t0 = now_seconds();
    for (int i = 0; i < threads; i++)
        ts.emplace_back([&] {
            bool s = true; // shared sense_ starts false
            for (int e = 0; e < episodes; e++) {
                bar.arrive(s);
                s = !s;
            }
        });
    for (auto& t : ts) t.join();
    return now_seconds() - t0;
}

#ifdef HAVE_PTHREAD_BARRIER
double run_once_pthread(PthreadBarrier& bar, int threads, int episodes) {
    std::vector<std::thread> ts;
    double t0 = now_seconds();
    for (int i = 0; i < threads; i++)
        ts.emplace_back([&] {
            for (int e = 0; e < episodes; e++) bar.arrive();
        });
    for (auto& t : ts) t.join();
    return now_seconds() - t0;
}
#endif
double run_once_std(StdBarrier& bar, int threads, int episodes) {
    std::vector<std::thread> ts;
    double t0 = now_seconds();
    for (int i = 0; i < threads; i++)
        ts.emplace_back([&] {
            for (int e = 0; e < episodes; e++) bar.arrive();
        });
    for (auto& t : ts) t.join();
    return now_seconds() - t0;
}

int main(int argc, char** argv) {
    std::string alg = get_str_arg(argc, argv, "algorithm", "srb");
    int threads = get_int_arg(argc, argv, "threads", 4);
    int episodes = get_int_arg(argc, argv, "episodes", 100000);
    int reps = get_int_arg(argc, argv, "reps", 1);
    std::string platform = get_str_arg(argc, argv, "platform", "unknown");

    for (int r = 0; r < reps; r++) {
        double dt = 0;
        if (alg == "srb")              { SenseBarrier b(threads);        dt = run_once(b, threads, episodes); }
        else if (alg == "srb_backoff") { SenseBarrierBackoff b(threads); dt = run_once(b, threads, episodes); }
        else if (alg == "std_barrier") { StdBarrier b(threads);          dt = run_once_std(b, threads, episodes); }
#ifdef HAVE_PTHREAD_BARRIER
        else if (alg == "pthread_barrier") { PthreadBarrier b(threads);  dt = run_once_pthread(b, threads, episodes); }
#endif
        else { fprintf(stderr, "unknown algorithm: %s\n", alg.c_str()); return 2; }

        csv_row(platform, 2, alg, threads, 0, "episodes_per_sec",
                episodes / dt, r);
        csv_row(platform, 2, alg, threads, 0, "wall_seconds", dt, r);
    }
    return 0;
}
