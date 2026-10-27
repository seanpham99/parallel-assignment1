#!/usr/bin/env python3
"""run_benchmarks.py — build, test, and benchmark Assignment 1 on this platform.

Usage:
    python3 run_benchmarks.py [--skip-tests] [--skip-mpi] [--quick]

Produces results/results_<platform>.csv with raw rows:
    platform,task,algorithm,threads,outside_work,metric,value,repetition
"""
import argparse
import os
import platform as pf
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(HERE, "build")
RESULTS = os.path.join(HERE, "results")

THREADS = [1, 2, 4, 8, 16]
OUTSIDE_WORK = [0, 1, 2, 4, 8]
REPS = 5

LOCK_ALGS = ["tas", "ttas", "ttas_backoff", "mcs", "std_mutex", "pthread_mutex"]
BARRIER_ALGS = ["srb", "srb_backoff", "std_barrier", "pthread_barrier"]
COUNTER_ALGS = ["mutex", "cas", "cas_backoff", "faa"]

if pf.system() == "Windows":
    LOCK_ALGS = [a for a in LOCK_ALGS if a != "pthread_mutex"]
    BARRIER_ALGS = [a for a in BARRIER_ALGS if a != "pthread_barrier"]
elif pf.system() == "Darwin":
    # macOS lacks pthread_barrier_t (unimplemented optional POSIX API).
    BARRIER_ALGS = [a for a in BARRIER_ALGS if a != "pthread_barrier"]

CSV_HEADER = "platform,task,algorithm,threads,outside_work,metric,value,repetition"


def platform_name() -> str:
    sysname = {"Darwin": "macos", "Linux": "linux", "Windows": "windows"}.get(
        pf.system(), pf.system().lower())
    return f"{sysname}-{pf.machine()}"


def run(cmd, **kw):
    print("  $", " ".join(cmd))
    return subprocess.run(cmd, **kw)


def build() -> int:
    os.makedirs(BUILD, exist_ok=True)
    r = run(["cmake", "-B", BUILD, "-S", HERE, "-DCMAKE_BUILD_TYPE=Release"])
    if r.returncode:
        return r.returncode
    return run(["cmake", "--build", BUILD, "-j", str(os.cpu_count() or 4)]).returncode


def exe(name: str) -> str:
    suffix = ".exe" if pf.system() == "Windows" else ""
    return os.path.join(BUILD, name + suffix)


def collect(proc, out, timeout_s=120) -> int:
    """Collect subprocess stdout: CSV rows -> file. Kill on timeout."""
    try:
        stdout, _ = proc.communicate(timeout=timeout_s)
    except subprocess.TimeoutExpired:
        proc.kill()
        stdout, _ = proc.communicate()
        print(f"  TIMEOUT {timeout_s}s (mpi)", file=sys.stderr)
    out.write(stdout)
    out.flush()
    return proc.returncode


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--skip-tests", action="store_true")
    ap.add_argument("--skip-mpi", action="store_true")
    ap.add_argument("--quick", action="store_true", help="1 rep, small sweep")
    ap.add_argument("--only", choices=["task1", "task2", "task3", "task4"],
                    default=None, help="run only one task's sweep")
    ap.add_argument("--append", action="store_true",
                    help="append to existing CSV instead of overwriting")
    args = ap.parse_args()

    plat = platform_name()
    reps = 1 if args.quick else REPS
    os.makedirs(RESULTS, exist_ok=True)
    out_path = os.path.join(RESULTS, f"results_{plat}.csv")

    print(f"== platform: {plat} ==")
    print("== build ==")
    if build():
        print("BUILD FAILED", file=sys.stderr)
        return 1

    if not args.skip_tests:
        print("== correctness tests ==")
        for t in ["test_locks", "test_barrier", "test_counters"]:
            if not os.path.exists(exe(t)):
                print(f"missing {t}, skipping")
                continue
            r = run([exe(t)])
            if r.returncode:
                print(f"{t} FAILED", file=sys.stderr)
                return 1

    append = args.append and os.path.exists(out_path)
    out = open(out_path, "a" if append else "w")
    if not append:
        out.write(CSV_HEADER + "\n")

    def bench(binary, extra, timeout_s=120):
        cmd = [exe(binary)] + extra + ["--reps=1", f"--platform={plat}"]
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True)
        try:
            stdout, _ = p.communicate(timeout=timeout_s)
        except subprocess.TimeoutExpired:
            p.kill()
            stdout, _ = p.communicate()
            print(f"  TIMEOUT {timeout_s}s: {' '.join(extra)}",
                  file=sys.stderr)
        out.write(stdout)
        out.flush()

    # ---- Task 1: locks ----
    # Keep total critical sections ~constant across thread counts: iters scale.
    if args.only in (None, "task1"):
        print("== task1: locks ==")
        for alg in LOCK_ALGS:
            for t in THREADS:
                for ow in OUTSIDE_WORK:
                    total_cs = 20000 if not args.quick else 2000
                    iters = max(1, total_cs // t)
                    for _ in range(reps):
                        bench("bench_locks",
                              [f"--algorithm={alg}", f"--threads={t}",
                               f"--outside_work={ow}", f"--iterations={iters}"])

    # ---- Task 2: barrier ----
    if args.only in (None, "task2"):
        print("== task2: barrier ==")
        for alg in BARRIER_ALGS:
            for t in THREADS:
                # Episodes scaled so even convoyed oversubscribed configs
                # finish inside the runner timeout: at t=8/16 on a 4-HT
                # machine a preempted arriver can stall an episode ~10-20ms.
                episodes = max(800, (8000 if not args.quick else 1000) // t)
                for _ in range(reps):
                    bench("bench_barrier",
                          [f"--algorithm={alg}", f"--threads={t}",
                           f"--episodes={episodes}"])

    # ---- Task 3: counters ----
    if args.only in (None, "task3"):
        print("== task3: counters ==")
        for alg in COUNTER_ALGS:
            for t in THREADS:
                for ow in OUTSIDE_WORK:
                    total_ops = 50000 if not args.quick else 5000
                    iters = max(1, total_ops // t)
                    for _ in range(reps):
                        bench("bench_counters",
                              [f"--algorithm={alg}", f"--threads={t}",
                               f"--outside_work={ow}", f"--iterations={iters}"])

    # ---- Task 4: MPI (optional) ----
    mpirun = shutil.which("mpirun") or shutil.which("mpiexec")
    # OpenMPI's PRRTE defaults slots to physical cores; --use-hwthread-cpus
    # lets -np 4 run on a 2-core/4-HT machine. MPICH/IntelMPI don't need it.
    mpi_extra = []
    if mpirun:
        v = subprocess.run([mpirun, "--version"], capture_output=True,
                           text=True).stdout
        if "Open MPI" in v:
            mpi_extra = ["--use-hwthread-cpus"]
    want4 = args.only in (None, "task4")
    if want4 and not args.skip_mpi and mpirun and os.path.exists(exe("bench_mpi")):
        print("== task4: MPI ==")
        for np_ in [1, 2, 4]:
            for _ in range(reps):
                p = subprocess.Popen(
                    [mpirun, *mpi_extra, "-np", str(np_), exe("bench_mpi"),
                     "--mode=counter", "--iterations=20000",
                     f"--platform={plat}"],
                    stdout=subprocess.PIPE, text=True)
                collect(p, out)
            for _ in range(reps):
                p = subprocess.Popen(
                    [mpirun, *mpi_extra, "-np", str(np_), exe("bench_mpi"),
                     "--mode=lock", "--iterations=2000",
                     f"--platform={plat}"],
                    stdout=subprocess.PIPE, text=True)
                collect(p, out)
            for _ in range(reps):
                p = subprocess.Popen(
                    [mpirun, *mpi_extra, "-np", str(np_), exe("bench_mpi"),
                     "--mode=barrier", "--episodes=20000",
                     f"--platform={plat}"],
                    stdout=subprocess.PIPE, text=True)
                collect(p, out)
    elif want4 and not args.skip_mpi:
        print("== task4: MPI skipped (mpirun or bench_mpi unavailable) ==")

    out.close()
    print(f"== done -> {out_path} ==")
    return 0


if __name__ == "__main__":
    sys.exit(main())
