// Task 4 benchmark (optional): MPI-3 one-sided distributed primitives.
// Modes:
//   --mode=counter --iterations=I   each rank issues I MPI_Fetch_and_op(+1)
//                                   to a counter in rank 0's window
//   --mode=lock    --iterations=I   CAS spinlock on rank 0's window; each
//                                   acquisition also bumps a shared counter
//                                   under the lock (correctness checkable)
//   --mode=barrier --episodes=E     MPI_Barrier throughput
// Rank 0 prints CSV rows. Run via: mpirun -np P bench_mpi --mode=... 
#include <mpi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <chrono>

static int get_iarg(int argc, char** argv, const char* name, int def) {
    std::string key = std::string("--") + name + "=";
    for (int i = 1; i < argc; i++)
        if (strncmp(argv[i], key.c_str(), key.size()) == 0)
            return atoi(argv[i] + key.size());
    return def;
}
static std::string get_sarg(int argc, char** argv, const char* name,
                            const char* def) {
    std::string key = std::string("--") + name + "=";
    for (int i = 1; i < argc; i++)
        if (strncmp(argv[i], key.c_str(), key.size()) == 0)
            return argv[i] + key.size();
    return def;
}
static double now_s() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

// Distributed CAS spinlock on rank 0 window slot `lock_disp`.
// Window layout on rank 0: [0]=counter  [1]=lock
static void rma_lock(MPI_Win win) {
    int zero = 0, one = 1, result;
    while (true) {
        MPI_Compare_and_swap(&one, &zero, &result, MPI_INT, 0, 1, win);
        MPI_Win_flush(0, win);
        if (result == 0) return; // acquired: previous value was 0
    }
}
static void rma_unlock(MPI_Win win) {
    int zero = 0;
    MPI_Accumulate(&zero, 1, MPI_INT, 0, 1, 1, MPI_INT, MPI_REPLACE, win);
    MPI_Win_flush(0, win);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank, np;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &np);

    std::string mode = get_sarg(argc, argv, "mode", "counter");
    int iters = get_iarg(argc, argv, "iterations", 10000);
    int episodes = get_iarg(argc, argv, "episodes", 10000);
    int reps = get_iarg(argc, argv, "reps", 1);
    std::string platform = get_sarg(argc, argv, "platform", "unknown");

    // Window of 2 ints on rank 0.
    MPI_Win win;
    int* base = nullptr;
    MPI_Win_allocate(rank == 0 ? 2 * sizeof(int) : 0, sizeof(int),
                     MPI_INFO_NULL, MPI_COMM_WORLD, &base, &win);
    if (rank == 0) { base[0] = 0; base[1] = 0; }
    MPI_Win_fence(0, win);
    MPI_Win_lock_all(0, win); // passive-target epochs for the whole run

    for (int r = 0; r < reps; r++) {
        MPI_Barrier(MPI_COMM_WORLD);
        double t0 = now_s();

        if (mode == "counter") {
            int one = 1, junk;
            for (int i = 0; i < iters; i++)
                MPI_Fetch_and_op(&one, &junk, MPI_INT, 0, 0, MPI_SUM, win);
            MPI_Win_flush(0, win);
            MPI_Barrier(MPI_COMM_WORLD);
            double dt = now_s() - t0;
            if (rank == 0) {
                MPI_Win_sync(win);
                long long total = base[0];
                printf("%s,4,mpi_faa_counter,%d,0,wall_seconds,%.6f,%d\n",
                       platform.c_str(), np, dt, r);
                printf("%s,4,mpi_faa_counter,%d,0,ops_per_sec,%.2f,%d\n",
                       platform.c_str(), np, np * iters / dt, r);
                fprintf(stderr, "verify counter=%lld expected=%lld %s\n", total,
                        (long long)np * iters * (r + 1),
                        total == (long long)np * iters * (r + 1) ? "OK"
                                                                 : "MISMATCH");
            }
        } else if (mode == "lock") {
            for (int i = 0; i < iters; i++) {
                rma_lock(win);
                int one = 1, junk;
                MPI_Fetch_and_op(&one, &junk, MPI_INT, 0, 0, MPI_SUM, win);
                MPI_Win_flush(0, win);
                rma_unlock(win);
            }
            MPI_Barrier(MPI_COMM_WORLD);
            double dt = now_s() - t0;
            if (rank == 0) {
                MPI_Win_sync(win);
                printf("%s,4,mpi_cas_lock,%d,0,wall_seconds,%.6f,%d\n",
                       platform.c_str(), np, dt, r);
                printf("%s,4,mpi_cas_lock,%d,0,ops_per_sec,%.2f,%d\n",
                       platform.c_str(), np, np * iters / dt, r);
                fprintf(stderr, "verify counter=%d expected=%lld %s\n", base[0],
                        (long long)np * iters * (r + 1),
                        base[0] == np * iters * (r + 1) ? "OK" : "MISMATCH");
            }
        } else if (mode == "barrier") {
            for (int i = 0; i < episodes; i++) MPI_Barrier(MPI_COMM_WORLD);
            double dt = now_s() - t0;
            if (rank == 0) {
                printf("%s,4,mpi_barrier,%d,0,episodes_per_sec,%.2f,%d\n",
                       platform.c_str(), np, episodes / dt, r);
            }
        } else {
            if (rank == 0) fprintf(stderr, "unknown mode %s\n", mode.c_str());
        }
    }

    MPI_Win_unlock_all(win);
    MPI_Win_free(&win);
    MPI_Finalize();
    return 0;
}
