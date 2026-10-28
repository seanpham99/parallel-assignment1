# Test Platform: Linux (Sean)

Collected: `lscpu`, `free`, `g++ --version`, `mpirun --version` — 2026-10-27.

| Property | Value |
|---|---|
| Model | Workstation (custom) |
| CPU | Intel Xeon W-2255 @ 3.70 GHz (Cascade Lake, 14 nm) |
| Cores / HW threads | 10 physical / 20 (Hyper-Threading) |
| L1d / L1i | 32 KiB + 32 KiB per core (320 KiB total) |
| L2 | 1 MiB per core (10 MiB total) |
| L3 | 19.3 MiB shared |
| RAM | 61 GiB |
| OS | Ubuntu 26.04.1 LTS (Resolute Raccoon), Linux x86_64 |
| Compiler | GCC 15.2.0 (g++), C++20 |
| MPI | OpenMPI 5.0.9 (source build, `--disable-mpi-fortran`) |
| Memory model | x86-64 TSO |

## Consequences for results

- Threads {1..16} never oversubscribe 20 hardware threads; spin-based
  primitives are not preempted by the OS scheduler, so MCS/TTAS-backoff
  results reflect true contention rather than convoy effects.
- `pthread_barrier_t` is available (Linux implements the optional POSIX
  barrier API); it is the pthread comparator for Task 2.
- MPI Task 4 runs natively (no source-build workarounds needed beyond the
  documented `--disable-mpi-fortran` build).
