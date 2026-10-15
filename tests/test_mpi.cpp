// Task 4 correctness: MPI-3 one-sided primitives.
// - Every rank performs N MPI_Fetch_and_op(+1) to rank 0's window counter;
//   final value must equal np*N exactly.
// - Ranks then do N locked increments via the CAS spinlock on the same window;
//   the counter must reach 2*np*N (any loss = lock or RMA broken).
// Run: mpirun -np P ./test_mpi [iters]
#include <mpi.h>
#include <cstdio>
#include <cstdlib>

static void rma_lock(MPI_Win win) {
    int zero = 0, one = 1, result;
    while (true) {
        MPI_Compare_and_swap(&one, &zero, &result, MPI_INT, 0, 1, win);
        MPI_Win_flush(0, win);
        if (result == 0) return;
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
    int iters = argc > 1 ? atoi(argv[1]) : 500;

    MPI_Win win;
    int* base = nullptr;
    MPI_Win_allocate(rank == 0 ? 2 * sizeof(int) : 0, sizeof(int),
                     MPI_INFO_NULL, MPI_COMM_WORLD, &base, &win);
    if (rank == 0) { base[0] = 0; base[1] = 0; }
    MPI_Win_fence(0, win);
    MPI_Win_lock_all(0, win);

    // Phase 1: pure fetch-and-add.
    int one = 1, junk;
    for (int i = 0; i < iters; i++)
        MPI_Fetch_and_op(&one, &junk, MPI_INT, 0, 0, MPI_SUM, win);
    MPI_Win_flush(0, win);
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        MPI_Win_sync(win);
        printf("faa counter=%d expected=%d %s\n", base[0], np * iters,
               base[0] == np * iters ? "PASS" : "FAIL");
    }

    // Phase 2: increments under the distributed CAS lock.
    for (int i = 0; i < iters; i++) {
        rma_lock(win);
        MPI_Fetch_and_op(&one, &junk, MPI_INT, 0, 0, MPI_SUM, win);
        MPI_Win_flush(0, win);
        rma_unlock(win);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
        MPI_Win_sync(win);
        printf("cas lock counter=%d expected=%d %s\n", base[0], 2 * np * iters,
               base[0] == 2 * np * iters ? "PASS" : "FAIL");
    }

    MPI_Win_unlock_all(win);
    MPI_Win_free(&win);
    MPI_Finalize();
    return 0;
}
