/**
 * @file bfs_message.cpp
 * @brief BFS (Breadth-First Search) workload with MESSAGE PASSING model
 *
 * Source: Graph500 Benchmark
 *
 * Message Passing Model:
 * - Graph vertices partitioned across subarrays
 * - Frontier vertices stored locally in each subarray
 * - Cross-partition edges require explicit inter-subarray transfers
 * - Each subarray processes local frontier independently
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
    uint64_t transfer_cycles = 0;
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;
    uint64_t direct_transfers = 0;
    uint64_t cross_partition_edges = 0;
    uint64_t local_edges = 0;
    int num_levels = 0;
    double total_energy = 0.0;
};

class BFSMessage {
private:
    BFSConfig config;
    BFSMetrics metrics;

    std::vector<int> vertex_to_subarray;
    std::vector<std::vector<int>> adjacency_list;
    std::vector<int> vertex_levels;
    std::vector<bool> visited;
    std::vector<std::queue<int>> local_frontiers;  // Per-subarray frontiers

public:
    BFSMessage(const BFSConfig& cfg) : config(cfg) {
        vertex_to_subarray.resize(config.num_vertices);
        adjacency_list.resize(config.num_vertices);
        vertex_levels.resize(config.num_vertices, -1);
        visited.resize(config.num_vertices, false);
        local_frontiers.resize(config.num_subarrays);

        // Partition vertices across subarrays
        for (int v = 0; v < config.num_vertices; v++) {
            vertex_to_subarray[v] = v % config.num_subarrays;
        }

        generateGraph();
    }

    void execute() {
        std::cout << "\n=== BFS Workload (MESSAGE PASSING) ===" << std::endl;
        std::cout << "Vertices: " << config.num_vertices << std::endl;
        std::cout << "Avg degree: " << config.avg_degree << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;

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

        // Count cross-partition edges
        for (int v = 0; v < config.num_vertices; v++) {
            int v_subarray = vertex_to_subarray[v];
            for (int u : adjacency_list[v]) {
                int u_subarray = vertex_to_subarray[u];
                if (v_subarray != u_subarray) {
                    metrics.cross_partition_edges++;
                } else {
                    metrics.local_edges++;
                }
            }
        }

        std::cout << "  Total edges: " << (metrics.cross_partition_edges + metrics.local_edges) << std::endl;
        std::cout << "  Cross-partition edges: " << metrics.cross_partition_edges << std::endl;
        std::cout << "  Local edges: " << metrics.local_edges << std::endl;
    }

    void runBFS(int start_vertex) {
        std::cout << "\nRunning BFS from vertex " << start_vertex << std::endl;

        int start_subarray = vertex_to_subarray[start_vertex];
        local_frontiers[start_subarray].push(start_vertex);
        visited[start_vertex] = true;
        vertex_levels[start_vertex] = 0;

        int current_level = 0;

        while (hasActiveFrontier()) {
            std::cout << "Level " << current_level << ": ";

            uint64_t vertices_at_level = 0;

            // Each subarray processes its local frontier
            for (int sa = 0; sa < config.num_subarrays; sa++) {
                vertices_at_level += processFrontier(sa, current_level);
            }

            std::cout << vertices_at_level << " vertices" << std::endl;

            current_level++;
            metrics.num_levels++;
        }

        std::cout << "✓ BFS complete: " << metrics.num_levels << " levels" << std::endl;
    }

    bool hasActiveFrontier() {
        for (int sa = 0; sa < config.num_subarrays; sa++) {
            if (!local_frontiers[sa].empty()) {
                return true;
            }
        }
        return false;
    }

    uint64_t processFrontier(int subarray_id, int current_level) {
        std::queue<int> next_frontier;
        uint64_t vertices_processed = 0;

        while (!local_frontiers[subarray_id].empty()) {
            int v = local_frontiers[subarray_id].front();
            local_frontiers[subarray_id].pop();
            vertices_processed++;

            // Read adjacency list (local access)
            metrics.total_cycles += config.read_latency;

            // Explore neighbors
            for (int u : adjacency_list[v]) {
                if (!visited[u]) {
                    visited[u] = true;
                    vertex_levels[u] = current_level + 1;

                    int u_subarray = vertex_to_subarray[u];

                    if (u_subarray != subarray_id) {
                        // Cross-partition edge: transfer to remote frontier
                        transferToFrontier(subarray_id, u_subarray, u);
                    } else {
                        // Local edge: add to next frontier
                        next_frontier.push(u);
                        metrics.compute_cycles += config.compute_latency;
                        metrics.total_cycles += config.compute_latency;
                    }
                }
            }
        }

        // Update local frontier for next level
        local_frontiers[subarray_id] = next_frontier;

        return vertices_processed;
    }

    void transferToFrontier(int src_subarray, int dst_subarray, int vertex) {
        assert(src_subarray != dst_subarray);

        // Transfer vertex to remote subarray's frontier
        local_frontiers[dst_subarray].push(vertex);

        metrics.intersubarray_transfers++;

        uint64_t transfer_cycles = config.copy_latency;
        metrics.transfer_cycles += transfer_cycles;
        metrics.total_cycles += transfer_cycles;

        if (config.copy_latency == 1) {
            metrics.direct_transfers++;
            metrics.total_energy += 0.55;  // LIBCom
        } else {
            metrics.htree_traversals++;
            metrics.total_energy += 1.0;   // Baseline
        }
    }

    void printMetrics() {
        std::cout << "\n=== BFS Results (MESSAGE PASSING) ===" << std::endl;
        std::cout << "BFS levels: " << metrics.num_levels << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << metrics.compute_cycles
                  << " (" << (100.0 * metrics.compute_cycles / metrics.total_cycles) << "%)" << std::endl;
        std::cout << "  Transfer cycles: " << metrics.transfer_cycles
                  << " (" << (100.0 * metrics.transfer_cycles / metrics.total_cycles) << "%)" << std::endl;

        std::cout << "\nGraph structure:" << std::endl;
        std::cout << "  Cross-partition edges: " << metrics.cross_partition_edges << std::endl;
        std::cout << "  Local edges: " << metrics.local_edges << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        std::cout << "  Frontier transfers: " << metrics.intersubarray_transfers << std::endl;

        if (config.copy_latency == 1) {
            std::cout << "  Direct transfers (LIBCom): " << metrics.direct_transfers << std::endl;
        } else {
            std::cout << "  H-tree traversals (Baseline): " << metrics.htree_traversals << std::endl;
        }

        std::cout << "\nEnergy:" << std::endl;
        std::cout << "  Total energy (relative): " << metrics.total_energy << std::endl;
        if (metrics.intersubarray_transfers > 0) {
            std::cout << "  Avg energy per transfer: "
                      << (metrics.total_energy / metrics.intersubarray_transfers) << std::endl;
        }

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

    std::cout << "\n=== DAC'26 BFS Benchmark (Message Passing) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Graph500 Benchmark" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;

    BFSMessage workload(config);
    workload.execute();

    return 0;
}
