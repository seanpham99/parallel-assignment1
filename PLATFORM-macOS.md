# Test Platform: macOS (Sean)

Collected: `sysctl`, `sw_vers` — 2026-10-15.

| Property | Value |
|---|---|
| Model | MacBook Air (Intel) |
| CPU | Intel Core i5-8210Y @ 1.60 GHz (Amber Lake, 14 nm) |
| Cores / HW threads | 2 physical / 4 (Hyper-Threading) |
| L1d / L1i | 32 KiB + 32 KiB per core |
| L2 | 256 KiB per core |
| L3 | 4 MiB shared |
| RAM | 8 GiB |
| OS | macOS 14.8.9 (23J631), Darwin 23.6.0 x86_64 |
| Compiler | Apple Clang 16.0.0 (clang-1600.0.26.6), C++20 |
| Memory model | x86-64 TSO |

## Consequences for results

- Threads {8, 16} oversubscribe 4 hardware threads 2×/4×. Spin-based
  primitives convoy (preempted waiter/holder stalls everyone); blocking
  primitives (std::mutex, std::barrier) degrade gracefully.
- macOS does not implement `pthread_barrier_t` — comparator for Task 2 on
  this platform is `std::barrier` only.
- MPI built from source (OpenMPI 5.0.9, `--disable-mpi-fortran`): MacPorts
  `openmpi-clang16` requires building gcc15/gfortran from source (multi-hour
  on this CPU); Task 4 uses only the C MPI API.
