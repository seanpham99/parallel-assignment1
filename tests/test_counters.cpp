// Task 3 correctness: threads perform equal increments and decrements;
// final value must be exactly 0. Any lost update shows up as drift.
#include "common.h"
#include "counters.h"

#include <cstdio>
#include <thread>
#include <vector>

static int THREADS = 4;
static int OPS = 20000;

template <typename Counter>
bool test_counter(const char* name) {
    Counter c;
    std::vector<std::thread> ts;
    for (int i = 0; i < THREADS; i++)
        ts.emplace_back([&, i] {
            for (int k = 0; k < OPS; k++) {
                c.add(i % 2 ? 1 : -1);
                c.add(i % 2 ? -1 : 1);
            }
        });
    for (auto& t : ts) t.join();
    bool ok = (c.get() == 0);
    printf("%-16s final=%lld expected=0 %s\n", name, c.get(),
           ok ? "PASS" : "FAIL");
    return ok;
}

int main(int argc, char** argv) {
    THREADS = get_int_arg(argc, argv, "threads", 4);
    OPS = get_int_arg(argc, argv, "iters", 20000);
    setvbuf(stdout, nullptr, _IOLBF, 0);
    bool ok = true;
    ok &= test_counter<MutexCounter>("mutex");
    ok &= test_counter<CASCounter>("cas");
    ok &= test_counter<CASBackoffCounter>("cas_backoff");
    ok &= test_counter<FAACounter>("faa");
    printf(ok ? "ALL PASS\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
