/*
 * MPICH-based Vector Dot Product for PIMID
 * Demonstrates host-side and device-side (PIM) execution
 *
 * This workload performs distributed vector dot product using MPI
 * with PIM annotations for device-side offloading
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>

// PIM annotation macros (based on PIMID's annotation approach)
#define PIM_OFFLOAD_START _Pragma("pim offload begin")
#define PIM_OFFLOAD_END _Pragma("pim offload end")

// Configuration
#define VECTOR_SIZE 1000000
#define USE_PIM 1  // Set to 0 for host-only execution

// Function to initialize vector with random values
void init_vector(double *vec, int size, int rank) {
    for (int i = 0; i < size; i++) {
        vec[i] = (double)(rank + 1) * (i + 1) * 0.001;
    }
}

// Host-side dot product computation
double host_dotproduct(double *a, double *b, int size) {
    double local_sum = 0.0;

    for (int i = 0; i < size; i++) {
        local_sum += a[i] * b[i];
    }

    return local_sum;
}

// PIM-side dot product computation (with annotations)
double pim_dotproduct(double *a, double *b, int size) {
    double local_sum = 0.0;

    // Mark this region for PIM execution
    PIM_OFFLOAD_START;
    {
        // This computation will be offloaded to PIM processing elements
        for (int i = 0; i < size; i++) {
            local_sum += a[i] * b[i];
        }
    }
    PIM_OFFLOAD_END;

    return local_sum;
}

int main(int argc, char *argv[]) {
    int rank, size;
    int local_size;
    double *local_a, *local_b;
    double local_result, global_result;
    double start_time, end_time;

    // Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Calculate local vector size for each process
    local_size = VECTOR_SIZE / size;

    // Allocate local vectors
    local_a = (double *)malloc(local_size * sizeof(double));
    local_b = (double *)malloc(local_size * sizeof(double));

    if (!local_a || !local_b) {
        fprintf(stderr, "Rank %d: Memory allocation failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Initialize local vectors
    init_vector(local_a, local_size, rank);
    init_vector(local_b, local_size, rank);

    // Synchronize before computation
    MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();

    // Compute local dot product (on host or PIM based on configuration)
    if (USE_PIM) {
        if (rank == 0) {
            printf("Using PIM-side computation\n");
        }
        local_result = pim_dotproduct(local_a, local_b, local_size);
    } else {
        if (rank == 0) {
            printf("Using host-side computation\n");
        }
        local_result = host_dotproduct(local_a, local_b, local_size);
    }

    // Reduce local results to get global dot product
    MPI_Reduce(&local_result, &global_result, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    // Synchronize after computation
    MPI_Barrier(MPI_COMM_WORLD);
    end_time = MPI_Wtime();

    // Print results
    if (rank == 0) {
        printf("===== Vector Dot Product Results =====\n");
        printf("Vector size: %d\n", VECTOR_SIZE);
        printf("Number of MPI processes: %d\n", size);
        printf("Execution mode: %s\n", USE_PIM ? "PIM-side" : "Host-side");
        printf("Global dot product result: %.6f\n", global_result);
        printf("Execution time: %.6f seconds\n", end_time - start_time);
        printf("======================================\n");
    }

    // Cleanup
    free(local_a);
    free(local_b);

    MPI_Finalize();
    return 0;
}
