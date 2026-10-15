// Shared benchmark/test utilities for Assignment 1.
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// On Windows MSVC we use __int64 for "long long" portability; keep it simple.
using i64 = long long;

// ---------------- is_prime workload ----------------
// is_prime(n) for n ~ 2^40 costs ~1-20us: trial division to sqrt(n) is 1M iterations,
// too slow. Instead we test primality of odd numbers in a fixed small range: pick
// numbers near ~3e5 -> sqrt ~ 548 -> ~274 iterations, ~1-2us per call. Tunable.
inline bool is_prime(i64 n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (i64 i = 3; i * i <= n; i += 2)
        if (n % i == 0) return false;
    return true;
}

// Prevent the compiler from deleting the loop / hoisting results.
inline volatile bool g_prime_sink = false;

// One unit of "work": test one odd candidate; rotate candidates so each call
// differs (avoids trivial hoisting), keep ~same cost.
inline void do_prime_unit(uint64_t& state) {
    // LCG-ish advance to next odd candidate in a bounded range.
    state = (state * 6364136223846793005ULL + 1442695040888963407ULL);
    i64 cand = static_cast<i64>((state >> 33) % 400000) | 1; // odd, < ~4e5
    g_prime_sink = is_prime(cand);
}

// ---------------- tiny arg parser ----------------
struct Args {
    int threads = 4;
    int outside_work = 0;   // extra work-units outside critical section
    int iterations = 0;     // work units per thread (task-dependent default)
    int episodes = 0;       // barrier: number of barrier episodes
    std::string algorithm = "tas";
    int reps = 1;
    std::string platform;
    int task = 0;
};

inline int get_int_arg(int argc, char** argv, const char* name, int def) {
    std::string key = std::string("--") + name + "=";
    for (int i = 1; i < argc; i++)
        if (strncmp(argv[i], key.c_str(), key.size()) == 0)
            return atoi(argv[i] + key.size());
    return def;
}

inline std::string get_str_arg(int argc, char** argv, const char* name, const char* def) {
    std::string key = std::string("--") + name + "=";
    for (int i = 1; i < argc; i++)
        if (strncmp(argv[i], key.c_str(), key.size()) == 0)
            return argv[i] + key.size();
    return def;
}

// ---------------- CSV row emitter ----------------
// Schema: platform,task,algorithm,threads,outside_work,metric,value,repetition
inline void csv_row(const std::string& platform, int task, const std::string& alg,
                    int threads, int outside_work, const char* metric,
                    double value, int rep) {
    printf("%s,%d,%s,%d,%d,%s,%.6f,%d\n", platform.c_str(), task, alg.c_str(),
           threads, outside_work, metric, value, rep);
}

inline double now_seconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}
