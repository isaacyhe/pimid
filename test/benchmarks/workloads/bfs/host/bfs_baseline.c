/*
 * Parallel BFS - Host-Only Baseline Version
 * Uses MPICH for distributed computation (no PIM annotations)
 * For comparison benchmarking against device-side version
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <time.h>

#define MAX_NODES 10000
#define AVG_EDGES_PER_NODE 10

typedef struct {
    int num_nodes;
    int num_edges;
    int *row_ptr;      // CSR format: row pointers
    int *col_idx;      // CSR format: column indices
} Graph;

typedef struct {
    int *visited;
    int *distance;
    int *frontier;
    int *next_frontier;
    int frontier_size;
    int next_frontier_size;
} BFSState;

// Generate random graph in CSR format
void generate_graph(Graph *graph, int num_nodes, int rank, int size) {
    graph->num_nodes = num_nodes;
    int edges_per_proc = (num_nodes * AVG_EDGES_PER_NODE) / size;

    graph->row_ptr = (int*)malloc((num_nodes + 1) * sizeof(int));
    graph->col_idx = (int*)malloc(edges_per_proc * 2 * sizeof(int));

    srand(time(NULL) + rank);

    int edge_count = 0;
    for (int i = 0; i < num_nodes; i++) {
        graph->row_ptr[i] = edge_count;

        // Generate random edges for this node
        int num_neighbors = (rand() % (AVG_EDGES_PER_NODE * 2)) + 1;
        for (int j = 0; j < num_neighbors && edge_count < edges_per_proc * 2; j++) {
            int neighbor = rand() % num_nodes;
            graph->col_idx[edge_count++] = neighbor;
        }
    }
    graph->row_ptr[num_nodes] = edge_count;
    graph->num_edges = edge_count;
}

// Initialize BFS state
void init_bfs_state(BFSState *state, int num_nodes, int source) {
    state->visited = (int*)calloc(num_nodes, sizeof(int));
    state->distance = (int*)malloc(num_nodes * sizeof(int));
    state->frontier = (int*)malloc(num_nodes * sizeof(int));
    state->next_frontier = (int*)malloc(num_nodes * sizeof(int));

    for (int i = 0; i < num_nodes; i++) {
        state->distance[i] = -1;
    }

    state->visited[source] = 1;
    state->distance[source] = 0;
    state->frontier[0] = source;
    state->frontier_size = 1;
    state->next_frontier_size = 0;
}

// BFS kernel (host-only, no PIM offloading)
void bfs_step(Graph *graph, BFSState *state, int rank, int size) {
    state->next_frontier_size = 0;

    // Process current frontier on host CPU
    for (int i = 0; i < state->frontier_size; i++) {
        int node = state->frontier[i];

        // Skip if not in this processor's partition
        if (node % size != rank) continue;

        // Explore neighbors
        int start = graph->row_ptr[node];
        int end = graph->row_ptr[node + 1];

        for (int j = start; j < end; j++) {
            int neighbor = graph->col_idx[j];

            if (!state->visited[neighbor]) {
                state->visited[neighbor] = 1;
                state->distance[neighbor] = state->distance[node] + 1;
                state->next_frontier[state->next_frontier_size++] = neighbor;
            }
        }
    }
}

// Free graph memory
void free_graph(Graph *graph) {
    free(graph->row_ptr);
    free(graph->col_idx);
}

// Free BFS state memory
void free_bfs_state(BFSState *state) {
    free(state->visited);
    free(state->distance);
    free(state->frontier);
    free(state->next_frontier);
}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int num_nodes = (argc > 1) ? atoi(argv[1]) : 1000;
    int source = 0;

    if (rank == 0) {
        printf("=== Parallel BFS with MPICH (Host-Only Baseline) ===\n");
        printf("Number of nodes: %d\n", num_nodes);
        printf("Number of processes: %d\n", size);
        printf("Source node: %d\n", source);
    }

    // Generate graph
    Graph graph;
    generate_graph(&graph, num_nodes, rank, size);

    if (rank == 0) {
        printf("Graph generated: %d edges\n", graph.num_edges);
    }

    // Initialize BFS state
    BFSState state;
    init_bfs_state(&state, num_nodes, source);

    MPI_Barrier(MPI_COMM_WORLD);
    double start_time = MPI_Wtime();

    // BFS iterations
    int level = 0;
    int global_frontier_size = 1;

    while (global_frontier_size > 0) {
        // Perform BFS step
        bfs_step(&graph, &state, rank, size);

        // Gather frontier sizes
        int local_frontier_size = state.next_frontier_size;
        MPI_Allreduce(&local_frontier_size, &global_frontier_size,
                      1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

        // Exchange visited information
        MPI_Allreduce(MPI_IN_PLACE, state.visited, num_nodes,
                      MPI_INT, MPI_LOR, MPI_COMM_WORLD);

        // Swap frontiers
        int *temp = state.frontier;
        state.frontier = state.next_frontier;
        state.next_frontier = temp;
        state.frontier_size = state.next_frontier_size;
        state.next_frontier_size = 0;

        level++;

        if (rank == 0 && level % 10 == 0) {
            printf("Level %d: frontier size = %d\n", level, global_frontier_size);
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end_time = MPI_Wtime();

    // Calculate statistics
    int visited_count = 0;
    for (int i = 0; i < num_nodes; i++) {
        if (state.visited[i]) visited_count++;
    }

    int global_visited_count;
    MPI_Reduce(&visited_count, &global_visited_count, 1, MPI_INT,
               MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("\n=== Results ===\n");
        printf("BFS completed in %d levels\n", level - 1);
        printf("Total nodes visited: %d / %d\n", global_visited_count / size, num_nodes);
        printf("Execution time: %.6f seconds\n", end_time - start_time);
        printf("Throughput: %.2f million edges/sec\n",
               (graph.num_edges * size / (end_time - start_time)) / 1e6);
    }

    // Cleanup
    free_graph(&graph);
    free_bfs_state(&state);

    MPI_Finalize();
    return 0;
}
