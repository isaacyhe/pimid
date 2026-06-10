/**
 * @file bfs_iterative_cosim.cpp
 * @brief TRUE Host/Device Co-Simulation: Iterative BFS
 *
 * HOST (OOO core with cache):
 *   - Generates or loads graph
 *   - Manages frontier queue between iterations
 *   - Checks convergence
 *   - Displays BFS tree levels
 *
 * DEVICE (ALU cores without cache):
 *   - Explores neighbors of frontier nodes in parallel
 *   - Updates distances for discovered nodes
 *   - Each PE handles a subset of frontier
 *
 * Communication: Shared memory for graph and BFS state
 * Pattern: Iterative host-device cooperation
 */

#include <iostream>
#include <vector>
#include <queue>
#include <cstdlib>
#include <cstring>
#include "zsim_hooks.h"
#include "../cosim_pe_parallel.h"

const int INF = -1;  // Unvisited nodes

struct Graph {
    int num_vertices;
    int num_edges;
    int** adjacency_list;  // adjacency_list[v] = array of neighbors
    int* neighbor_counts;  // Number of neighbors for each vertex
};

struct BFSState {
    Graph graph;
    int source_vertex;
    int* distances;         // Distance from source to each vertex
    bool* visited;          // Visited flags
    std::vector<int> current_frontier;
    std::vector<int> next_frontier;
    int current_level;
    int num_device_pes;
    std::vector<int>* pe_discoveries;  // Nodes discovered by each PE
};

// ============================================================================
// HOST CODE (runs on OOO/Simple core with cache)
// ============================================================================

class HostBFSCoordinator {
private:
    BFSState& state;

public:
    HostBFSCoordinator(BFSState& bfs_state) : state(bfs_state) {}

    // HOST: Generate random graph
    void generateGraph() {
        std::cout << "[HOST] Generating random graph" << std::endl;
        std::cout << "[HOST]   Vertices: " << state.graph.num_vertices << std::endl;
        std::cout << "[HOST]   Avg degree: ~5" << std::endl;

        state.graph.adjacency_list = new int*[state.graph.num_vertices];
        state.graph.neighbor_counts = new int[state.graph.num_vertices];

        state.graph.num_edges = 0;
        for (int v = 0; v < state.graph.num_vertices; v++) {
            // Random degree between 3-8
            int degree = 3 + (rand() % 6);
            state.graph.neighbor_counts[v] = degree;
            state.graph.adjacency_list[v] = new int[degree];

            // Random neighbors
            for (int i = 0; i < degree; i++) {
                state.graph.adjacency_list[v][i] = rand() % state.graph.num_vertices;
                state.graph.num_edges++;
            }
        }

        std::cout << "[HOST]   Total edges: " << state.graph.num_edges << std::endl;
    }

    // HOST: Initialize BFS state
    void initializeBFS() {
        std::cout << "[HOST] Initializing BFS from source vertex "
                  << state.source_vertex << std::endl;

        state.distances = new int[state.graph.num_vertices];
        state.visited = new bool[state.graph.num_vertices];

        for (int v = 0; v < state.graph.num_vertices; v++) {
            state.distances[v] = INF;
            state.visited[v] = false;
        }

        // Initialize source
        state.distances[state.source_vertex] = 0;
        state.visited[state.source_vertex] = true;
        state.current_frontier.push_back(state.source_vertex);
        state.current_level = 0;

        // Allocate PE discovery arrays
        state.pe_discoveries = new std::vector<int>[state.num_device_pes];
    }

    // HOST: Coordinate iteration
    bool hasWorkForIteration() {
        return !state.current_frontier.empty();
    }

    void startIteration() {
        std::cout << "[HOST] Level " << state.current_level
                  << ": Exploring " << state.current_frontier.size()
                  << " frontier nodes" << std::endl;

        state.next_frontier.clear();
        for (int pe = 0; pe < state.num_device_pes; pe++) {
            state.pe_discoveries[pe].clear();
        }
    }

    // HOST: Prepare next frontier
    void prepareNextFrontier() {
        std::cout << "[HOST] Processing device discoveries..." << std::endl;

        // HOST: Collect all discovered nodes from all PEs
        for (int pe = 0; pe < state.num_device_pes; pe++) {
            for (int node : state.pe_discoveries[pe]) {
                if (!state.visited[node]) {
                    state.visited[node] = true;
                    state.distances[node] = state.current_level + 1;
                    state.next_frontier.push_back(node);
                }
            }
        }

        std::cout << "[HOST] Discovered " << state.next_frontier.size()
                  << " new nodes at level " << (state.current_level + 1)
                  << std::endl;

        // Advance to next level
        state.current_frontier = state.next_frontier;
        state.current_level++;
    }

    // HOST: Display BFS results
    void displayResults() {
        std::cout << "[HOST] BFS Complete!" << std::endl;
        std::cout << "[HOST] Total levels: " << state.current_level << std::endl;

        // Count nodes at each level
        std::vector<int> level_counts(state.current_level + 1, 0);
        int reachable = 0;

        for (int v = 0; v < state.graph.num_vertices; v++) {
            if (state.distances[v] != INF) {
                level_counts[state.distances[v]]++;
                reachable++;
            }
        }

        std::cout << "[HOST] Reachable vertices: " << reachable << "/"
                  << state.graph.num_vertices << std::endl;

        std::cout << "[HOST] Nodes per level:" << std::endl;
        for (int level = 0; level <= state.current_level; level++) {
            std::cout << "[HOST]   Level " << level << ": "
                      << level_counts[level] << " nodes" << std::endl;
        }
    }

    // HOST: Cleanup
    void cleanup() {
        std::cout << "[HOST] Cleaning up graph and BFS state" << std::endl;

        for (int v = 0; v < state.graph.num_vertices; v++) {
            delete[] state.graph.adjacency_list[v];
        }
        delete[] state.graph.adjacency_list;
        delete[] state.graph.neighbor_counts;
        delete[] state.distances;
        delete[] state.visited;
        delete[] state.pe_discoveries;
    }
};

// ============================================================================
// DEVICE CODE (runs on ALU cores without cache)
// ============================================================================

class DeviceBFSExplorer {
private:
    int pe_id;
    BFSState& state;

public:
    DeviceBFSExplorer(int id, BFSState& bfs_state)
        : pe_id(id), state(bfs_state) {}

    // DEVICE: Explore assigned frontier nodes
    void exploreFrontier() {
        int frontier_size = state.current_frontier.size();
        int chunk_size = (frontier_size + state.num_device_pes - 1) /
                        state.num_device_pes;
        int start = pe_id * chunk_size;
        int end = std::min(start + chunk_size, frontier_size);

        // DEVICE: Parallel neighbor exploration
        for (int i = start; i < end; i++) {
            int vertex = state.current_frontier[i];
            exploreNeighbors(vertex);
        }

        if (pe_id == 0 && frontier_size > 0) {
            std::cout << "[DEVICE PE-" << pe_id << "] Explored frontier nodes "
                      << start << " to " << end << std::endl;
        }
    }

private:
    // DEVICE: Explore neighbors of a vertex
    void exploreNeighbors(int vertex) {
        int num_neighbors = state.graph.neighbor_counts[vertex];
        int* neighbors = state.graph.adjacency_list[vertex];

        // DEVICE: Check each neighbor
        for (int i = 0; i < num_neighbors; i++) {
            int neighbor = neighbors[i];

            // Discover unvisited neighbors
            if (!state.visited[neighbor]) {
                state.pe_discoveries[pe_id].push_back(neighbor);
            }
        }
    }
};

// ============================================================================
// DEVICE KERNEL (offloaded via pimid_offload_sync)
// ============================================================================

// SERIAL variant: no device parallelism. A single device worker explores every
// PE's frontier slice inside the offload region via a plain loop (no pthreads), so
// all work is attributed to one device context rather than N PEs.
static void device_kernel(void* raw) {
    BFSState* state = (BFSState*)raw;
    for (int i = 0; i < state->num_device_pes; i++) {
        DeviceBFSExplorer pe(i, *state);
        pe.exploreFrontier();
    }
}

// ============================================================================
// MAIN: Orchestrates Host/Device Co-Simulation
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <num_vertices> <source_vertex> <num_device_pes>" << std::endl;
        return 1;
    }

    int num_vertices = std::atoi(argv[1]);
    int source = std::atoi(argv[2]);
    int num_pes = std::atoi(argv[3]);

    if (source >= num_vertices) source = 0;

    std::cout << "=== HOST/DEVICE CO-SIMULATION: Iterative BFS ===" << std::endl;
    std::cout << "Graph vertices: " << num_vertices << std::endl;
    std::cout << "Source vertex: " << source << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    BFSState bfs_state;
    bfs_state.graph.num_vertices = num_vertices;
    bfs_state.source_vertex = source;
    bfs_state.num_device_pes = num_pes;

    // HOST: Setup
    HostBFSCoordinator host(bfs_state);
    host.generateGraph();
    host.initializeBFS();

    std::cout << std::endl;
    std::cout << "--- STARTING ITERATIVE BFS ---" << std::endl;
    std::cout << std::endl;

    // ITERATIVE HOST-DEVICE COOPERATION
    int iteration = 0;
    const int MAX_ITERATIONS = 100;

    while (host.hasWorkForIteration() && iteration < MAX_ITERATIONS) {
        // HOST: Coordinate iteration
        host.startIteration();

        // DEVICE: Offload frontier exploration to ALU cores via ZSim hooks
        pimid_offload_sync(device_kernel, &bfs_state);

        // HOST: Prepare next iteration
        host.prepareNextFrontier();

        iteration++;
        std::cout << std::endl;
    }

    std::cout << "--- BFS ITERATIONS COMPLETE ---" << std::endl;
    std::cout << std::endl;

    // HOST: Display results
    host.displayResults();
    host.cleanup();

    std::cout << std::endl;
    std::cout << "=== Co-Simulation Complete ===" << std::endl;

    return 0;
}
