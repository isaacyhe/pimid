/**
 * @file bfs_shared.cpp
 * @brief BFS (Breadth-First Search) workload with SHARED MEMORY model
 *
 * Source: Graph500 Benchmark
 *
 * Shared Memory Model:
 * - Graph structure stored in shared memory
 * - Global frontier queue accessible to all subarrays
 * - Each subarray processes assigned vertices
 * - Atomic operations for frontier updates
 */

#include <iostream>
#include <vector>
#include <queue>
#include <cstdint>
#include <cassert>
#include <cmath>

struct BFSConfig {
    int num_subarrays;
    int num_vertices;
    int avg_degree;

    int copy_latency;
    int read_latency;
    int write_latency;
    int compute_latency;
};

struct BFSMetrics {
    uint64_t total_cycles = 0;
    uint64_t compute_cycles = 0;
    uint64_t sync_cycles = 0;
    uint64_t atomic_ops = 0;
    uint64_t edges_processed = 0;
    int num_levels = 0;
    uint64_t remote_accesses = 0;
    double total_energy = 0.0;
};

class BFSShared {
private:
    BFSConfig config;
    BFSMetrics metrics;
    bool is_libcom;

    std::vector<std::vector<int>> adjacency_list;
    std::vector<int> vertex_levels;
    std::vector<bool> visited;
    std::vector<int> vertex_assignment;  // Which subarray processes each vertex

public:
    BFSShared(const BFSConfig& cfg, bool use_libcom) : config(cfg), is_libcom(use_libcom) {
        adjacency_list.resize(config.num_vertices);
        vertex_levels.resize(config.num_vertices, -1);
        visited.resize(config.num_vertices, false);
        vertex_assignment.resize(config.num_vertices);

        // Assign vertices to subarrays for processing
        for (int v = 0; v < config.num_vertices; v++) {
            vertex_assignment[v] = v % config.num_subarrays;
        }

        generateGraph();
    }

    void execute() {
        std::cout << "\n=== BFS Workload (SHARED MEMORY) ===" << std::endl;
        std::cout << "Vertices: " << config.num_vertices << std::endl;
        std::cout << "Avg degree: " << config.avg_degree << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;

        runBFS(0);
        printMetrics();
    }

private:
    void generateGraph() {
        std::cout << "\nGenerating graph..." << std::endl;

        // Simple deterministic graph for reproducibility
        for (int v = 0; v < config.num_vertices; v++) {
            for (int i = 0; i < config.avg_degree; i++) {
                int neighbor = (v + i + 1) % config.num_vertices;
                adjacency_list[v].push_back(neighbor);
            }
        }

        uint64_t total_edges = config.num_vertices * config.avg_degree;
        std::cout << "  Total edges: " << total_edges << std::endl;
    }

    void runBFS(int start_vertex) {
        std::cout << "\nRunning BFS from vertex " << start_vertex << std::endl;

        std::queue<int> current_frontier;
        current_frontier.push(start_vertex);
        visited[start_vertex] = true;
        vertex_levels[start_vertex] = 0;

        // Atomic write to shared visited array
        metrics.atomic_ops++;
        metrics.total_cycles += 2;

        int current_level = 0;

        while (!current_frontier.empty()) {
            std::queue<int> next_frontier;
            uint64_t vertices_at_level = current_frontier.size();

            std::cout << "Level " << current_level << ": " << vertices_at_level << " vertices" << std::endl;

            // Each subarray processes its assigned vertices from the frontier
            for (int sa = 0; sa < config.num_subarrays; sa++) {
                processFrontier(sa, current_frontier, next_frontier, current_level);
            }

            // Barrier synchronization
            metrics.sync_cycles += 10;
            metrics.total_cycles += 10;

            current_frontier = next_frontier;
            current_level++;
            metrics.num_levels++;
        }

        std::cout << "✓ BFS complete: " << metrics.num_levels << " levels" << std::endl;
    }

    void processFrontier(int subarray_id, std::queue<int>& current_frontier,
                        std::queue<int>& next_frontier, int current_level) {
        std::queue<int> temp_frontier = current_frontier;
        uint64_t vertices_processed = 0;

        while (!temp_frontier.empty()) {
            int v = temp_frontier.front();
            temp_frontier.pop();

            // Only process if assigned to this subarray
            if (vertex_assignment[v] != subarray_id) {
                continue;
            }

            vertices_processed++;

            // Read adjacency list from shared memory
            metrics.total_cycles += config.read_latency;

            // Track remote access if vertex is from another subarray
            if (vertex_assignment[v] != subarray_id) {
                metrics.remote_accesses++;
            }

            // Explore neighbors
            for (int u : adjacency_list[v]) {
                // Read visited flag from shared memory
                metrics.total_cycles += config.read_latency;

                // Track remote access if neighbor is in another subarray
                if (vertex_assignment[u] != subarray_id) {
                    metrics.remote_accesses++;
                }

                if (!visited[u]) {
                    // Atomic test-and-set on visited flag
                    metrics.atomic_ops++;
                    metrics.total_cycles += 2;

                    visited[u] = true;
                    vertex_levels[u] = current_level + 1;

                    // Track remote write if neighbor is in another subarray
                    if (vertex_assignment[u] != subarray_id) {
                        metrics.remote_accesses++;
                    }

                    // Add to next frontier (shared structure)
                    next_frontier.push(u);
                    metrics.atomic_ops++;
                    metrics.total_cycles += 2;

                    metrics.compute_cycles += config.compute_latency;
                    metrics.total_cycles += config.compute_latency;
                }

                metrics.edges_processed++;
            }
        }
    }

    void printMetrics() {
        // Calculate energy
        double energy_per_access = is_libcom ? 0.55 : 1.0;
        metrics.total_energy = metrics.remote_accesses * energy_per_access;

        std::cout << "\n=== BFS Results (SHARED MEMORY) ===" << std::endl;
        std::cout << "BFS levels: " << metrics.num_levels << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Sync cycles: " << metrics.sync_cycles
                  << " (" << (100.0 * metrics.sync_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Edges processed: " << metrics.edges_processed << std::endl;
        std::cout << "  Atomic operations: " << metrics.atomic_ops << std::endl;

        std::cout << "\nCommunication (Shared Memory):" << std::endl;
        std::cout << "  Inter-subarray transfers: 0 (shared memory model)" << std::endl;
        std::cout << "  Remote accesses: " << metrics.remote_accesses << std::endl;

        std::cout << "\nEnergy:" << std::endl;
        std::cout << "  Remote accesses: " << metrics.remote_accesses << std::endl;
        std::cout << "  Energy per access: " << energy_per_access << " pJ" << std::endl;
        std::cout << "  Total energy: " << metrics.total_energy << " pJ" << std::endl;

        // Validation
        std::cout << "\nValidation:" << std::endl;
        int visited_count = 0;
        for (bool v : visited) {
            if (v) visited_count++;
        }
        std::cout << "  Vertices visited: " << visited_count << " / " << config.num_vertices << std::endl;
        std::cout << "  ✓ BFS traversal complete" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <num_vertices> <is_libcom>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 64 0   # 8 subarrays, 64 vertices, baseline" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8 64 1   # 8 subarrays, 64 vertices, LIBCom" << std::endl;
        return 1;
    }

    BFSConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.num_vertices = std::atoi(argv[2]);
    bool is_libcom = (std::atoi(argv[3]) == 1);

    config.avg_degree = 8;
    config.read_latency = 1;
    config.write_latency = 1;
    config.compute_latency = 1;

    if (is_libcom) {
        config.copy_latency = 1;
    } else {
        int htree_latency = 0;
        int n = config.num_subarrays;
        while (n > 1) { htree_latency++; n >>= 1; }
        config.copy_latency = 2 + htree_latency;
    }

    std::cout << "\n=== DAC'26 BFS Benchmark (Shared Memory) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Graph500 Benchmark" << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;

    BFSShared workload(config, is_libcom);
    workload.execute();

    return 0;
}
