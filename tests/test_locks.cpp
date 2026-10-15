// Task 1 correctness: shared non-atomic counter under each lock must reach
// threads * iterations exactly. Any race drops increments.
#include "common.h"
#include "locks.h"

#include <cstdio>
#include <thread>
#include <vector>

static int THREADS = 8;
static int ITERS = 20000;

template <typename Lock>
bool test_lock(const char* name) {
    Lock lock;
    long long counter = 0;
    std::vector<std::thread> ts;
    for (int i = 0; i < THREADS; i++)
        ts.emplace_back([&] {
            for (int k = 0; k < ITERS; k++) {
                lock.lock();
                counter++;
                lock.unlock();
            }
        });
    for (auto& t : ts) t.join();
    bool ok = (counter == (long long)THREADS * ITERS);
    printf("%-16s counter=%lld expected=%lld %s\n", name, counter,
           (long long)THREADS * ITERS, ok ? "PASS" : "FAIL");
    return ok;
}

bool test_mcs() {
    MCSLock lock;
    long long counter = 0;
    std::vector<std::thread> ts;
    for (int i = 0; i < THREADS; i++)
        ts.emplace_back([&] {
            MCSLock::Node me;
            for (int k = 0; k < ITERS; k++) {
                lock.lock(me);
                counter++;
                lock.unlock(me);
            }
        });
    for (auto& t : ts) t.join();
    bool ok = (counter == (long long)THREADS * ITERS);
    printf("%-16s counter=%lld expected=%lld %s\n", "mcs", counter,
           (long long)THREADS * ITERS, ok ? "PASS" : "FAIL");
    return ok;
}

int main(int argc, char** argv) {
    THREADS = get_int_arg(argc, argv, "threads", 4);
    ITERS = get_int_arg(argc, argv, "iters", 5000);
    setvbuf(stdout, nullptr, _IOLBF, 0);
    bool ok = true;
    ok &= test_lock<TASLock>("tas");
    ok &= test_lock<TTASLock>("ttas");
    ok &= test_lock<TTASBackoffLock>("ttas_backoff");
    ok &= test_mcs();
    ok &= test_lock<StdMutexLock>("std_mutex");
#ifndef _WIN32
    ok &= test_lock<PthreadLock>("pthread_mutex");
#endif
    printf(ok ? "ALL PASS\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
