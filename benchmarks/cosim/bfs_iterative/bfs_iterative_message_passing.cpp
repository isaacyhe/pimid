/**
 * @file bfs_iterative_message_passing.cpp
 * @brief Host/Device co-simulation: iterative BFS, MESSAGE-PASSING model.
 *
 * Serial host manages the frontier across levels and offloads neighbor
 * exploration to N device PEs each level. Unlike the shared_memory variant (PEs
 * read the shared frontier vector and push into shared per-PE discovery vectors
 * through host pointers), here each PE runs FULL-DMA per level:
 *   (1) host->device  : DMA this PE's slice of the current frontier (the vertex
 *                       IDs it must explore) into a PRIVATE buffer via
 *                       pimid_pe_recv (charged on the host<->device link),
 *   (2) compute        : explore neighbors of its private frontier vertices,
 *                       accumulating a PRIVATE discovered-node set,
 *   (3) device->host  : DMA the discovered set back into this PE's host landing
 *                       buffer via pimid_pe_send (charged; size = #discoveries).
 * The frontier slice (the PE's per-iteration input) is never dereferenced from
 * host pointers in the compute loop — only the private copy is used. The graph
 * adjacency is a large read-only shared structure (consulted for neighbor
 * lookups), matching the kernel's stated input/output slices.
 *
 * Result correctness (per-level node counts, reachable set) is identical to the
 * shared_memory variant.
 */

#include <iostream>
#include <vector>
#include <queue>
#include <cstdlib>
#include <cstring>
#include "zsim_hooks.h"
#include "../cosim_pe_message.h"

const int INF = -1;  // Unvisited nodes

struct Graph {
    int num_vertices;
    int num_edges;
    int** adjacency_list;
    int* neighbor_counts;
};

struct BFSState {
    Graph graph;
    int source_vertex;
    int* distances;
    bool* visited;
    std::vector<int> current_frontier;
    std::vector<int> next_frontier;
    int current_level;
    int num_device_pes;
    std::vector<int>* pe_discoveries;   // host-side landing buffers (filled via messages)
    bool reported_dma;                  // print one DMA line on the first level only
};

// ============================================================================
// HOST CODE (runs on OOO/Simple core with cache)
// ============================================================================

class HostBFSCoordinator {
private:
    BFSState& state;

public:
    HostBFSCoordinator(BFSState& bfs_state) : state(bfs_state) {}

    void generateGraph() {
        std::cout << "[HOST] Generating random graph" << std::endl;
        std::cout << "[HOST]   Vertices: " << state.graph.num_vertices << std::endl;
        std::cout << "[HOST]   Avg degree: ~5" << std::endl;

        state.graph.adjacency_list = new int*[state.graph.num_vertices];
        state.graph.neighbor_counts = new int[state.graph.num_vertices];

        state.graph.num_edges = 0;
        for (int v = 0; v < state.graph.num_vertices; v++) {
            int degree = 3 + (rand() % 6);
            state.graph.neighbor_counts[v] = degree;
            state.graph.adjacency_list[v] = new int[degree];
            for (int i = 0; i < degree; i++) {
                state.graph.adjacency_list[v][i] = rand() % state.graph.num_vertices;
                state.graph.num_edges++;
            }
        }

        std::cout << "[HOST]   Total edges: " << state.graph.num_edges << std::endl;
    }

    void initializeBFS() {
        std::cout << "[HOST] Initializing BFS from source vertex "
                  << state.source_vertex << std::endl;

        state.distances = new int[state.graph.num_vertices];
        state.visited = new bool[state.graph.num_vertices];

        for (int v = 0; v < state.graph.num_vertices; v++) {
            state.distances[v] = INF;
            state.visited[v] = false;
        }

        state.distances[state.source_vertex] = 0;
        state.visited[state.source_vertex] = true;
        state.current_frontier.push_back(state.source_vertex);
        state.current_level = 0;
        state.reported_dma = false;

        state.pe_discoveries = new std::vector<int>[state.num_device_pes];
    }

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

    void prepareNextFrontier() {
        std::cout << "[HOST] Processing device discoveries..." << std::endl;

        for (int pe = 0; pe < state.num_device_pes; pe++) {
            for (size_t k = 0; k < state.pe_discoveries[pe].size(); k++) {
                int node = state.pe_discoveries[pe][k];
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

        state.current_frontier = state.next_frontier;
        state.current_level++;
    }

    void displayResults() {
        std::cout << "[HOST] BFS Complete!" << std::endl;
        std::cout << "[HOST] Total levels: " << state.current_level << std::endl;

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

    // HOST: verify the message-passing BFS against an independent sequential BFS.
    void verifyResult() {
        std::cout << "[HOST] Verifying BFS result..." << std::endl;
        std::vector<int> ref(state.graph.num_vertices, INF);
        std::queue<int> q;
        ref[state.source_vertex] = 0;
        q.push(state.source_vertex);
        while (!q.empty()) {
            int u = q.front(); q.pop();
            for (int i = 0; i < state.graph.neighbor_counts[u]; i++) {
                int w = state.graph.adjacency_list[u][i];
                if (ref[w] == INF) {
                    ref[w] = ref[u] + 1;
                    q.push(w);
                }
            }
        }
        int mismatches = 0;
        for (int v = 0; v < state.graph.num_vertices; v++) {
            if (state.distances[v] != ref[v]) mismatches++;
        }
        if (mismatches == 0) {
            std::cout << "[HOST] \xE2\x9C\x93 Verification passed! (distances match sequential BFS)"
                      << std::endl;
        } else {
            std::cout << "[HOST] \xE2\x9C\x97 Verification failed! " << mismatches
                      << " distance mismatches" << std::endl;
        }
    }

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
// DEVICE CODE — full-DMA per level: recv frontier slice, explore privately,
// send discovered set back.
// ============================================================================

static void device_kernel(void* raw) {
    BFSState* state = (BFSState*)raw;
    int n = state->num_device_pes;
    int frontier_size = (int)state->current_frontier.size();
    pimid_parallel_pes_msg(n, [&](int pe_id) {
        int chunk = (frontier_size + n - 1) / n;
        int start = pe_id * chunk;
        int end = std::min(start + chunk, frontier_size);
        int len = end - start;
        if (len <= 0) { return; }

        // FULL-DMA (1): host->device — copy this PE's frontier slice (vertex IDs)
        // into a PRIVATE device buffer (charged on the link).
        std::vector<int> frontier_priv(len);
        pimid_pe_recv(frontier_priv.data(), &state->current_frontier[start],
                      (unsigned)(len * sizeof(int)));

        // (2) explore neighbors of the PRIVATE frontier vertices, accumulating a
        // PRIVATE discovered-node set. (Graph adjacency is a read-only shared
        // structure consulted for neighbor lookups.)
        std::vector<int> discovered;
        for (int i = 0; i < len; i++) {
            int vertex = frontier_priv[i];
            int num_neighbors = state->graph.neighbor_counts[vertex];
            int* neighbors = state->graph.adjacency_list[vertex];
            for (int k = 0; k < num_neighbors; k++) {
                int neighbor = neighbors[k];
                if (!state->visited[neighbor]) {
                    discovered.push_back(neighbor);
                }
            }
        }

        // FULL-DMA (3): device->host — ship the discovered set back into this
        // PE's host landing buffer (charged; size = #discoveries).
        state->pe_discoveries[pe_id].resize(discovered.size());
        if (!discovered.empty()) {
            pimid_pe_send(state->pe_discoveries[pe_id].data(), discovered.data(),
                          (unsigned)(discovered.size() * sizeof(int)));
        }

        if (pe_id == 0 && !state->reported_dma) {
            state->reported_dma = true;
            std::cout << "[DEVICE PE-0] DMA in " << (len * sizeof(int))
                      << " B (frontier slice, " << len << " nodes), explored "
                      << start << " to " << end << ", DMA out "
                      << (discovered.size() * sizeof(int)) << " B ("
                      << discovered.size() << " discoveries)" << std::endl;
        }
    });
}

// ============================================================================
// MAIN
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

    std::cout << "=== HOST/DEVICE CO-SIM (message-passing): Iterative BFS ===" << std::endl;
    std::cout << "Graph vertices: " << num_vertices << std::endl;
    std::cout << "Source vertex: " << source << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    BFSState bfs_state;
    bfs_state.graph.num_vertices = num_vertices;
    bfs_state.source_vertex = source;
    bfs_state.num_device_pes = num_pes;

    HostBFSCoordinator host(bfs_state);
    host.generateGraph();
    host.initializeBFS();

    std::cout << "\n--- STARTING ITERATIVE BFS (PEs DMA frontier slice in, discoveries out) ---\n"
              << std::endl;

    int iteration = 0;
    const int MAX_ITERATIONS = 100;

    while (host.hasWorkForIteration() && iteration < MAX_ITERATIONS) {
        host.startIteration();
        pimid_offload_sync(device_kernel, &bfs_state);
        host.prepareNextFrontier();
        iteration++;
        std::cout << std::endl;
    }

    std::cout << "--- BFS ITERATIONS COMPLETE ---" << std::endl;
    std::cout << std::endl;

    host.displayResults();
    host.verifyResult();
    host.cleanup();

    std::cout << "\n=== Co-Simulation Complete ===" << std::endl;

    return 0;
}
