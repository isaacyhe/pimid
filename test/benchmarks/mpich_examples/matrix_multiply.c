/*
 * MPICH-based Matrix Multiplication for PIMID
 * Demonstrates distributed matrix multiplication with PIM offloading
 *
 * This workload performs parallel matrix multiplication (C = A * B)
 * using MPI for distribution and PIM annotations for device-side execution
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>
#include <math.h>

// PIM annotation macros
#define PIM_OFFLOAD_START _Pragma("pim offload begin")
#define PIM_OFFLOAD_END _Pragma("pim offload end")

// Configuration
#define MATRIX_SIZE 512  // N x N matrices
#define USE_PIM 1        // Set to 0 for host-only execution

// Function to initialize matrix with values
void init_matrix(double *matrix, int rows, int cols, int rank) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix[i * cols + j] = (double)((rank + 1) * (i + j + 1)) * 0.01;
        }
    }
}

// Host-side matrix multiplication
void host_matmul(double *local_a, double *b, double *local_c,
                 int local_rows, int n) {
    for (int i = 0; i < local_rows; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) {
                sum += local_a[i * n + k] * b[k * n + j];
            }
            local_c[i * n + j] = sum;
        }
    }
}

// PIM-side matrix multiplication (with annotations)
void pim_matmul(double *local_a, double *b, double *local_c,
                int local_rows, int n) {
    // Offload computation to PIM processing elements
    PIM_OFFLOAD_START;
    {
        // This memory-intensive computation benefits from near-memory processing
        for (int i = 0; i < local_rows; i++) {
            for (int j = 0; j < n; j++) {
                double sum = 0.0;
                // Inner loop accesses both matrices extensively
                // PIM reduces data movement between memory and processor
                for (int k = 0; k < n; k++) {
                    sum += local_a[i * n + k] * b[k * n + j];
                }
                local_c[i * n + j] = sum;
            }
        }
    }
    PIM_OFFLOAD_END;
}

// Verify result (simple checksum)
double compute_checksum(double *matrix, int rows, int cols) {
    double checksum = 0.0;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            checksum += matrix[i * cols + j];
        }
    }
    return checksum;
}

int main(int argc, char *argv[]) {
    int rank, num_procs;
    int n = MATRIX_SIZE;
    int local_rows;
    double *local_a, *b, *local_c;
    double *global_c = NULL;
    double start_time, end_time;
    double local_checksum, global_checksum;

    // Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    // Calculate rows per process
    if (n % num_procs != 0) {
        if (rank == 0) {
            fprintf(stderr, "Matrix size (%d) must be divisible by number of processes (%d)\n",
                    n, num_procs);
        }
        MPI_Finalize();
        return 1;
    }

    local_rows = n / num_procs;

    // Allocate memory
    local_a = (double *)malloc(local_rows * n * sizeof(double));
    b = (double *)malloc(n * n * sizeof(double));  // Full matrix B on all processes
    local_c = (double *)malloc(local_rows * n * sizeof(double));

    if (!local_a || !b || !local_c) {
        fprintf(stderr, "Rank %d: Memory allocation failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Initialize matrices
    init_matrix(local_a, local_rows, n, rank);
    init_matrix(b, n, n, rank);  // Each process has full B matrix

    if (rank == 0) {
        printf("===== Matrix Multiplication (MPICH + PIM) =====\n");
        printf("Matrix size: %d x %d\n", n, n);
        printf("Number of MPI processes: %d\n", num_procs);
        printf("Rows per process: %d\n", local_rows);
        printf("Execution mode: %s\n", USE_PIM ? "PIM-side" : "Host-side");
        printf("==============================================\n");
    }

    // Synchronize before computation
    MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();

    // Perform matrix multiplication
    if (USE_PIM) {
        pim_matmul(local_a, b, local_c, local_rows, n);
    } else {
        host_matmul(local_a, b, local_c, local_rows, n);
    }

    // Synchronize after computation
    MPI_Barrier(MPI_COMM_WORLD);
    end_time = MPI_Wtime();

    // Compute local checksum for verification
    local_checksum = compute_checksum(local_c, local_rows, n);

    // Gather results at rank 0 for verification
    if (rank == 0) {
        global_c = (double *)malloc(n * n * sizeof(double));
    }

    MPI_Gather(local_c, local_rows * n, MPI_DOUBLE,
               global_c, local_rows * n, MPI_DOUBLE,
               0, MPI_COMM_WORLD);

    // Reduce checksums
    MPI_Reduce(&local_checksum, &global_checksum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    // Print results
    if (rank == 0) {
        printf("\n===== Computation Results =====\n");
        printf("Execution time: %.6f seconds\n", end_time - start_time);
        printf("Performance: %.2f GFLOPS\n",
               (2.0 * n * n * n) / ((end_time - start_time) * 1e9));
        printf("Result checksum: %.6f\n", global_checksum);
        printf("Sample result C[0][0]: %.6f\n", global_c ? global_c[0] : 0.0);
        printf("Sample result C[%d][%d]: %.6f\n", n-1, n-1,
               global_c ? global_c[n*n-1] : 0.0);
        printf("==============================\n");

        free(global_c);
    }

    // Cleanup
    free(local_a);
    free(b);
    free(local_c);

    MPI_Finalize();
    return 0;
}
