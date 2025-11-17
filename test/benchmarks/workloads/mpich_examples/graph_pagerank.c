/*
 * MPICH-based Graph PageRank for PIMID
 * Demonstrates graph analytics with PIM offloading
 *
 * This workload performs distributed PageRank computation using MPI
 * with PIM annotations for memory-intensive graph traversal
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <math.h>
#include <string.h>

// PIM annotation macros
#define PIM_OFFLOAD_START _Pragma("pim offload begin")
#define PIM_OFFLOAD_END _Pragma("pim offload end")

// Configuration
#define NUM_VERTICES 10000
#define AVG_DEGREE 10
#define MAX_ITERATIONS 20
#define DAMPING_FACTOR 0.85
#define CONVERGENCE_THRESHOLD 0.0001
#define USE_PIM 1

// Graph structure (CSR format - Compressed Sparse Row)
typedef struct {
    int num_vertices;
    int num_edges;
    int *row_ptr;      // Row pointers
    int *col_idx;      // Column indices
    int *out_degree;   // Out-degree of each vertex
} Graph;

// Initialize random graph in CSR format
void init_graph(Graph *g, int num_vertices, int avg_degree, int rank) {
    g->num_vertices = num_vertices;
    g->num_edges = num_vertices * avg_degree;

    g->row_ptr = (int *)malloc((num_vertices + 1) * sizeof(int));
    g->col_idx = (int *)malloc(g->num_edges * sizeof(int));
    g->out_degree = (int *)malloc(num_vertices * sizeof(int));

    // Simple synthetic graph generation
    srand(rank + 42);
    int edge_idx = 0;

    for (int i = 0; i < num_vertices; i++) {
        g->row_ptr[i] = edge_idx;
        int degree = avg_degree + (rand() % 5) - 2;  // Vary degree slightly
        if (degree < 1) degree = 1;

        g->out_degree[i] = degree;

        for (int j = 0; j < degree && edge_idx < g->num_edges; j++) {
            g->col_idx[edge_idx] = rand() % num_vertices;
            edge_idx++;
        }
    }
    g->row_ptr[num_vertices] = edge_idx;
    g->num_edges = edge_idx;
}

// Host-side PageRank iteration
void host_pagerank_iteration(Graph *g, double *rank_curr, double *rank_next,
                             int local_start, int local_size) {
    // Reset next rank values
    for (int i = 0; i < g->num_vertices; i++) {
        rank_next[i] = (1.0 - DAMPING_FACTOR) / g->num_vertices;
    }

    // Compute PageRank for local vertices
    for (int i = local_start; i < local_start + local_size; i++) {
        int row_start = g->row_ptr[i];
        int row_end = g->row_ptr[i + 1];
        double contrib = DAMPING_FACTOR * rank_curr[i] / g->out_degree[i];

        // Distribute rank to neighbors
        for (int j = row_start; j < row_end; j++) {
            int neighbor = g->col_idx[j];
            rank_next[neighbor] += contrib;
        }
    }
}

// PIM-side PageRank iteration (with annotations)
void pim_pagerank_iteration(Graph *g, double *rank_curr, double *rank_next,
                            int local_start, int local_size) {
    // Reset next rank values
    for (int i = 0; i < g->num_vertices; i++) {
        rank_next[i] = (1.0 - DAMPING_FACTOR) / g->num_vertices;
    }

    // Offload memory-intensive graph traversal to PIM
    PIM_OFFLOAD_START;
    {
        // PIM benefits: irregular memory access patterns in graph traversal
        // Near-memory processing reduces data movement overhead
        for (int i = local_start; i < local_start + local_size; i++) {
            int row_start = g->row_ptr[i];
            int row_end = g->row_ptr[i + 1];
            double contrib = DAMPING_FACTOR * rank_curr[i] / g->out_degree[i];

            // Random memory accesses to neighbors - benefits from PIM
            for (int j = row_start; j < row_end; j++) {
                int neighbor = g->col_idx[j];
                rank_next[neighbor] += contrib;
            }
        }
    }
    PIM_OFFLOAD_END;
}

// Compute L1 norm for convergence check
double compute_diff(double *a, double *b, int size) {
    double diff = 0.0;
    for (int i = 0; i < size; i++) {
        diff += fabs(a[i] - b[i]);
    }
    return diff;
}

int main(int argc, char *argv[]) {
    int rank, num_procs;
    Graph graph;
    double *rank_curr, *rank_next, *rank_temp;
    double *global_rank_next = NULL;
    int local_size, local_start;
    double start_time, end_time;
    int iteration;
    double local_diff, global_diff;

    // Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    // Calculate vertex distribution
    local_size = NUM_VERTICES / num_procs;
    local_start = rank * local_size;

    // Initialize graph (all processes have full graph structure)
    init_graph(&graph, NUM_VERTICES, AVG_DEGREE, rank);

    // Allocate PageRank arrays
    rank_curr = (double *)malloc(NUM_VERTICES * sizeof(double));
    rank_next = (double *)malloc(NUM_VERTICES * sizeof(double));

    if (!rank_curr || !rank_next) {
        fprintf(stderr, "Rank %d: Memory allocation failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Initialize all vertices with equal rank
    for (int i = 0; i < NUM_VERTICES; i++) {
        rank_curr[i] = 1.0 / NUM_VERTICES;
        rank_next[i] = 0.0;
    }

    if (rank == 0) {
        printf("===== Graph PageRank (MPICH + PIM) =====\n");
        printf("Number of vertices: %d\n", NUM_VERTICES);
        printf("Number of edges: %d\n", graph.num_edges);
        printf("Average degree: %d\n", AVG_DEGREE);
        printf("Number of MPI processes: %d\n", num_procs);
        printf("Vertices per process: %d\n", local_size);
        printf("Execution mode: %s\n", USE_PIM ? "PIM-side" : "Host-side");
        printf("========================================\n");
    }

    // PageRank iterations
    MPI_Barrier(MPI_COMM_WORLD);
    start_time = MPI_Wtime();

    for (iteration = 0; iteration < MAX_ITERATIONS; iteration++) {
        // Perform local PageRank computation
        if (USE_PIM) {
            pim_pagerank_iteration(&graph, rank_curr, rank_next, local_start, local_size);
        } else {
            host_pagerank_iteration(&graph, rank_curr, rank_next, local_start, local_size);
        }

        // Aggregate results from all processes
        MPI_Allreduce(MPI_IN_PLACE, rank_next, NUM_VERTICES, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

        // Check convergence
        local_diff = compute_diff(rank_curr, rank_next, NUM_VERTICES);
        MPI_Reduce(&local_diff, &global_diff, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

        if (rank == 0 && iteration % 5 == 0) {
            printf("Iteration %d: diff = %.6f\n", iteration, global_diff);
        }

        // Swap rank arrays
        rank_temp = rank_curr;
        rank_curr = rank_next;
        rank_next = rank_temp;

        // Check convergence
        if (global_diff < CONVERGENCE_THRESHOLD) {
            if (rank == 0) {
                printf("Converged at iteration %d\n", iteration);
            }
            break;
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    end_time = MPI_Wtime();

    // Print results
    if (rank == 0) {
        printf("\n===== PageRank Results =====\n");
        printf("Total iterations: %d\n", iteration + 1);
        printf("Execution time: %.6f seconds\n", end_time - start_time);
        printf("Time per iteration: %.6f seconds\n", (end_time - start_time) / (iteration + 1));

        // Print top-ranked vertices
        printf("\nTop 5 vertices by PageRank:\n");
        for (int i = 0; i < 5 && i < NUM_VERTICES; i++) {
            double max_rank = 0.0;
            int max_idx = 0;
            for (int j = 0; j < NUM_VERTICES; j++) {
                if (rank_curr[j] > max_rank) {
                    max_rank = rank_curr[j];
                    max_idx = j;
                }
            }
            printf("  Vertex %d: %.6f\n", max_idx, max_rank);
            rank_curr[max_idx] = -1.0;  // Mark as processed
        }

        printf("============================\n");
    }

    // Cleanup
    free(graph.row_ptr);
    free(graph.col_idx);
    free(graph.out_degree);
    free(rank_curr);
    free(rank_next);

    MPI_Finalize();
    return 0;
}
