# Test Platform: Windows (Nitro AN515-54)

Collected: `systeminfo`, `Get-CimInstance Win32_Processor`, `g++ --version` — 2026-10-27.

| Property | Value |
|---|---|
| Model | Acer Nitro AN515-54 |
| CPU | Intel Core i5-9300H @ 2.40 GHz (Coffee Lake, 14 nm) |
| Cores / HW threads | 4 physical / 8 (Hyper-Threading) |
| L2 | 1 MiB (256 KiB per core) |
| L3 | 8 MiB shared |
| RAM | 32 GiB |
| OS | Windows 11 Home Single Language, build 26300, x64 |
| Compiler | MinGW-W64 GCC 13.2.0 (UCRT, POSIX threads, SEH), C++20 |
| Build system | CMake 3.29.2, MinGW Makefiles |
| MPI | not installed (Task 4 skipped) |
| Memory model | x86-64 TSO |

## Consequences for results

- Threads {8, 16} oversubscribe 8 hardware threads 1×/2×. Spin-based
  primitives convoy under preemption; blocking primitives degrade
  gracefully. Compare with the Linux (20 HT) and macOS (4 HT) runs.
- `pthread_barrier_t` / `pthread_mutex_t` are compile-guarded out
  (`_WIN32`): the pthread comparators are absent on this platform;
  `std::mutex` and `std::barrier` are the baselines instead.
- Task 4 (MPI-3 one-sided) is not run; no MS-MPI/Intel MPI is installed.
- MinGW provides POSIX threads via libwinpthread, so `std::thread` and
  the spinlocks build, but timing resolution and scheduler behaviour
  differ from native Linux pthreads.
