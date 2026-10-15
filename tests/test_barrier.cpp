// Task 2 correctness: step-lock test. In round R every thread checks that all
// threads completed round R-1 before it proceeds — a broken barrier violates it.
#include "common.h"
#include "barrier.h"

#include <cstdio>
#include <vector>
#include <thread>

static int THREADS = 4;
static int ROUNDS = 2000;

template <typename Barrier>
bool test_barrier(const char* name) {
    Barrier bar(THREADS);
    std::vector<std::atomic<int>> progress(THREADS);
    for (auto& p : progress) p.store(0);
    std::atomic<int> violations{0};

    std::vector<std::thread> ts;
    for (int i = 0; i < THREADS; i++)
        ts.emplace_back([&, i] {
            bool s = true;
            for (int r = 0; r < ROUNDS; r++) {
                progress[i].store(r + 1, std::memory_order_relaxed);
                bar.arrive(s);
                s = !s;
                // After the barrier, every thread must have finished round r.
                for (int j = 0; j < THREADS; j++)
                    if (progress[j].load(std::memory_order_relaxed) < r + 1)
                        violations++;
            }
        });
    for (auto& t : ts) t.join();
    bool ok = violations.load() == 0;
    printf("%-16s rounds=%d violations=%d %s\n", name, ROUNDS,
           violations.load(), ok ? "PASS" : "FAIL");
    return ok;
}

#ifdef HAVE_PTHREAD_BARRIER
bool test_pthread() {
    PthreadBarrier bar(THREADS);
    std::vector<std::atomic<int>> progress(THREADS);
    for (auto& p : progress) p.store(0);
    std::atomic<int> violations{0};
    std::vector<std::thread> ts;
    for (int i = 0; i < THREADS; i++)
        ts.emplace_back([&, i] {
            for (int r = 0; r < ROUNDS; r++) {
                progress[i].store(r + 1, std::memory_order_relaxed);
                bar.arrive();
                for (int j = 0; j < THREADS; j++)
                    if (progress[j].load(std::memory_order_relaxed) < r + 1)
                        violations++;
            }
        });
    for (auto& t : ts) t.join();
    bool ok = violations.load() == 0;
    printf("%-16s rounds=%d violations=%d %s\n", "pthread_barrier", ROUNDS,
           violations.load(), ok ? "PASS" : "FAIL");
    return ok;
}
#endif

bool test_std() {
    StdBarrier bar(THREADS);
    std::vector<std::atomic<int>> progress(THREADS);
    for (auto& p : progress) p.store(0);
    std::atomic<int> violations{0};
    std::vector<std::thread> ts;
    for (int i = 0; i < THREADS; i++)
        ts.emplace_back([&, i] {
            for (int r = 0; r < ROUNDS; r++) {
                progress[i].store(r + 1, std::memory_order_relaxed);
                bar.arrive();
                for (int j = 0; j < THREADS; j++)
                    if (progress[j].load(std::memory_order_relaxed) < r + 1)
                        violations++;
            }
        });
    for (auto& t : ts) t.join();
    bool ok = violations.load() == 0;
    printf("%-16s rounds=%d violations=%d %s\n", "std_barrier", ROUNDS,
           violations.load(), ok ? "PASS" : "FAIL");
    return ok;
}

int main(int argc, char** argv) {
    THREADS = get_int_arg(argc, argv, "threads", 4);
    ROUNDS = get_int_arg(argc, argv, "rounds", 2000);
    setvbuf(stdout, nullptr, _IOLBF, 0);
    bool ok = true;
    ok &= test_barrier<SenseBarrier>("srb");
    ok &= test_barrier<SenseBarrierBackoff>("srb_backoff");
    ok &= test_std();
#ifdef HAVE_PTHREAD_BARRIER
    ok &= test_pthread();
#endif
    printf(ok ? "ALL PASS\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
