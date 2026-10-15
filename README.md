# Assignment 1 — Parallel Computing

Mutex locks (TAS/TTAS/TTAS-backoff/MCS), sense-reversing barrier, concurrent
counters (mutex/CAS/CAS-backoff/FAA), and optional MPI-3 one-sided Task 4.

## Requirements

| Platform | Toolchain |
|---|---|
| macOS | Apple Clang (Xcode CLT), CMake; OpenMPI for Task 4 |
| Linux | GCC/Clang, CMake, pthreads; OpenMPI/MPICH for Task 4 |
| Windows | MSVC (VS 2022) or MinGW + CMake; MS-MPI/Intel MPI for Task 4 |

No OpenMP needed. C++20 required (`std::barrier`, `atomic_flag::test`).

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```
- `pthread_barrier` comparator is Linux-only — macOS never implemented the
  optional POSIX barrier API; there `std::barrier` is the baseline.

MPI is optional: `bench_mpi` builds only if `find_package(MPI)` succeeds.

## Run everything (correctness tests + benchmarks → CSV)

```bash
python3 run_benchmarks.py            # full sweep, 5 reps
python3 run_benchmarks.py --quick    # smoke: 1 rep, reduced workload
python3 run_benchmarks.py --skip-mpi # shared-memory tasks only
```

Output: `results/results_<platform>-<arch>.csv` — one raw row per repetition:
`platform,task,algorithm,threads,outside_work,metric,value,repetition`.

Teammates: run the same command on your machine, commit your CSV back.

## Plot

```bash
python3 plot.py   # needs matplotlib; prints median table without it
```

Reads every `results/results_*.csv`, writes PNGs to `plots/`.

## Layout

```
include/common.h    is_prime workload, arg parsing, CSV emitter
include/locks.h     TASLock, TTASLock, TTASBackoffLock, MCSLock, adapters
include/barrier.h   SenseBarrier, SenseBarrierBackoff, pthread/std adapters
include/counters.h  MutexCounter, CASCounter, CASBackoffCounter, FAACounter
tests/              correctness binaries (run first by run_benchmarks.py)
benchmarks/         bench_locks, bench_barrier, bench_counters
src/distributed/    bench_mpi — MPI_Fetch_and_op counter, MPI_Compare_and_swap
                    lock, MPI_Barrier (mpirun -np P bench_mpi --mode=…)
```

## Parameters (this run)

- Task 1: threads {1,2,4,8,16} × outside_work {0,1,2,4,8}; total critical
  sections fixed at 20000 per config (iterations = 20000/threads), 5 reps.
  Critical section = one `is_prime` unit (~1-2µs, odd candidate < 4e5).
- Task 2: 20000 barrier episodes per config, zero work between.
- Task 3: 50000 total increments per config (iterations = 50000/threads),
  same outside_work sweep, 5 reps.
- Task 4: mpirun -np {1,2,4}; counter 20000 FAA ops/rank; lock 2000
  acquisitions/rank; barrier 20000 episodes.
