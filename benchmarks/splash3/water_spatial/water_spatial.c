/* water_spatial.c -- Molecular dynamics with spatial hashing (cell lists)
 * SPLASH-3 style: pthreads parallel. Molecules assigned to cells in a 3D grid;
 * force computation only between molecules in same or neighboring cells.
 * Lennard-Jones potential with cutoff radius. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "zsim_hooks.h"

#define DEFAULT_SIZE    512
#define DEFAULT_THREADS 4
#define CUTOFF          2.5
#define BOX_SIZE        10.0
#define DT              0.001
#define NUM_STEPS       5
#define EPSILON         1.0
#define SIGMA           1.0
#define MAX_PER_CELL    64

static int parse_int_arg(int argc, char** argv, const char* flag, int def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

static uint32_t bench_rand(uint32_t* s) {
    *s = *s * 1103515245 + 12345;
    return (*s >> 16) & 0x7FFF;
}

typedef struct {
    double x, y, z;
    double vx, vy, vz;
    double fx, fy, fz;
} Molecule;

typedef struct {
    int indices[MAX_PER_CELL];
    int count;
} Cell;

/* Shared state */
static Molecule* mols;
static Cell* cells;
static int num_mols, ncells_dim, total_cells;
static double cell_size;
static int nthreads;
static pthread_barrier_t barrier;

static int cell_index(double coord) {
    int c = (int)(coord / cell_size);
    if (c < 0) c = 0;
    if (c >= ncells_dim) c = ncells_dim - 1;
    return c;
}

static void assign_to_cells(void) {
    for (int i = 0; i < total_cells; i++)
        cells[i].count = 0;
    for (int i = 0; i < num_mols; i++) {
        int cx = cell_index(mols[i].x);
        int cy = cell_index(mols[i].y);
        int cz = cell_index(mols[i].z);
        int ci = cx * ncells_dim * ncells_dim + cy * ncells_dim + cz;
        if (cells[ci].count < MAX_PER_CELL)
            cells[ci].indices[cells[ci].count++] = i;
    }
}

static void compute_pair_force(int i, int j) {
    double dx = mols[i].x - mols[j].x;
    double dy = mols[i].y - mols[j].y;
    double dz = mols[i].z - mols[j].z;
    double r2 = dx * dx + dy * dy + dz * dz;
    if (r2 < CUTOFF * CUTOFF && r2 > 1e-12) {
        double r2inv = SIGMA * SIGMA / r2;
        double r6inv = r2inv * r2inv * r2inv;
        double f = 24.0 * EPSILON * r6inv * (2.0 * r6inv - 1.0) / r2;
        mols[i].fx += f * dx;
        mols[i].fy += f * dy;
        mols[i].fz += f * dz;
    }
}

typedef struct {
    int tid;
} ThreadArg;

static void* worker(void* arg) {
    int tid = ((ThreadArg*)arg)->tid;

    for (int step = 0; step < NUM_STEPS; step++) {
        /* Phase 1: assign molecules to cells (thread 0 only) */
        if (tid == 0)
            assign_to_cells();
        pthread_barrier_wait(&barrier);

        /* Phase 2: zero forces for this thread's molecules */
        int mol_lo = (num_mols * tid) / nthreads;
        int mol_hi = (num_mols * (tid + 1)) / nthreads;
        for (int i = mol_lo; i < mol_hi; i++) {
            mols[i].fx = 0.0;
            mols[i].fy = 0.0;
            mols[i].fz = 0.0;
        }
        pthread_barrier_wait(&barrier);

        /* Phase 3: compute forces -- partition cells among threads */
        int cell_lo = (total_cells * tid) / nthreads;
        int cell_hi = (total_cells * (tid + 1)) / nthreads;
        for (int ci = cell_lo; ci < cell_hi; ci++) {
            int cx = ci / (ncells_dim * ncells_dim);
            int cy = (ci / ncells_dim) % ncells_dim;
            int cz = ci % ncells_dim;
            /* Interact with same cell and 26 neighbors */
            for (int dx = -1; dx <= 1; dx++) {
                int nx = cx + dx;
                if (nx < 0 || nx >= ncells_dim) continue;
                for (int dy = -1; dy <= 1; dy++) {
                    int ny = cy + dy;
                    if (ny < 0 || ny >= ncells_dim) continue;
                    for (int dz = -1; dz <= 1; dz++) {
                        int nz = cz + dz;
                        if (nz < 0 || nz >= ncells_dim) continue;
                        int ni = nx * ncells_dim * ncells_dim + ny * ncells_dim + nz;
                        for (int a = 0; a < cells[ci].count; a++) {
                            int mi = cells[ci].indices[a];
                            int start = (ci == ni) ? a + 1 : 0;
                            for (int b = start; b < cells[ni].count; b++) {
                                int mj = cells[ni].indices[b];
                                compute_pair_force(mi, mj);
                            }
                        }
                    }
                }
            }
        }
        pthread_barrier_wait(&barrier);

        /* Phase 4: velocity Verlet integration -- partition molecules */
        for (int i = mol_lo; i < mol_hi; i++) {
            mols[i].vx += mols[i].fx * DT;
            mols[i].vy += mols[i].fy * DT;
            mols[i].vz += mols[i].fz * DT;
            mols[i].x += mols[i].vx * DT;
            mols[i].y += mols[i].vy * DT;
            mols[i].z += mols[i].vz * DT;
            /* Periodic boundary */
            if (mols[i].x < 0) mols[i].x += BOX_SIZE;
            if (mols[i].x >= BOX_SIZE) mols[i].x -= BOX_SIZE;
            if (mols[i].y < 0) mols[i].y += BOX_SIZE;
            if (mols[i].y >= BOX_SIZE) mols[i].y -= BOX_SIZE;
            if (mols[i].z < 0) mols[i].z += BOX_SIZE;
            if (mols[i].z >= BOX_SIZE) mols[i].z -= BOX_SIZE;
        }
        pthread_barrier_wait(&barrier);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    num_mols = parse_int_arg(argc, argv, "--size", DEFAULT_SIZE);
    nthreads = parse_int_arg(argc, argv, "--threads", DEFAULT_THREADS);

    cell_size = CUTOFF;
    ncells_dim = (int)(BOX_SIZE / cell_size);
    if (ncells_dim < 1) ncells_dim = 1;
    total_cells = ncells_dim * ncells_dim * ncells_dim;

    mols = (Molecule*)malloc((size_t)num_mols * sizeof(Molecule));
    cells = (Cell*)malloc((size_t)total_cells * sizeof(Cell));
    if (!mols || !cells) { fprintf(stderr, "malloc failed\n"); return 1; }

    /* Deterministic init */
    uint32_t seed = 42;
    for (int i = 0; i < num_mols; i++) {
        mols[i].x = BOX_SIZE * bench_rand(&seed) / 32768.0;
        mols[i].y = BOX_SIZE * bench_rand(&seed) / 32768.0;
        mols[i].z = BOX_SIZE * bench_rand(&seed) / 32768.0;
        mols[i].vx = 0.01 * (bench_rand(&seed) / 32768.0 - 0.5);
        mols[i].vy = 0.01 * (bench_rand(&seed) / 32768.0 - 0.5);
        mols[i].vz = 0.01 * (bench_rand(&seed) / 32768.0 - 0.5);
        mols[i].fx = mols[i].fy = mols[i].fz = 0.0;
    }

    pthread_barrier_init(&barrier, NULL, (unsigned)nthreads);
    pthread_t* threads = (pthread_t*)malloc((size_t)nthreads * sizeof(pthread_t));
    ThreadArg* args = (ThreadArg*)malloc((size_t)nthreads * sizeof(ThreadArg));

    zsim_roi_begin();
    for (int t = 1; t < nthreads; t++) {
        args[t].tid = t;
        pthread_create(&threads[t], NULL, worker, &args[t]);
    }
    args[0].tid = 0;
    worker(&args[0]);
    for (int t = 1; t < nthreads; t++)
        pthread_join(threads[t], NULL);
    zsim_roi_end();

    /* Checksum: sum of final positions */
    double checksum = 0.0;
    for (int i = 0; i < num_mols; i++)
        checksum += mols[i].x + mols[i].y + mols[i].z;
    printf("BENCH_CHECKSUM: %f\n", checksum);
    printf("BENCH_DONE\n");

    pthread_barrier_destroy(&barrier);
    free(threads); free(args);
    free(mols); free(cells);
    return 0;
}
