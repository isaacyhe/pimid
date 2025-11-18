/**
 * @file bfs_workload.cpp
 * @brief BFS (Breadth-First Search) workload for LIBCom evaluation
 *
 * Graph vertices are partitioned across subarrays.
 * Tests cross-partition frontier expansion.
 */

#include <iostream>
#include <vector>
#include <queue>
#include <cstdint>
#include <cassert>

struct BFSConfig {
    int num_subarrays;
    int num_vertices;
    int avg_degree;            // Average edges per vertex
    int subarray_size_words;   // 1024 words per subarray

    int copy_latency;
    int move_latency;
    int read_latency;
    int write_latency;
};

struct BFSMetrics {
    uint64_t total_cycles = 0;
    uint64_t intersubarray_transfers = 0;
    uint64_t htree_traversals = 0;
    uint64_t direct_transfers = 0;
    uint64_t cross_partition_edges = 0;
    double total_energy = 0.0;
    int num_levels = 0;
};

class BFSWorkload {
private:
    BFSConfig config;
    BFSMetrics metrics;

    // Vertex partitioning
    std::vector<int> vertex_to_subarray;
    std::vector<std::vector<int>> adjacency_list;
    std::vector<int> vertex_levels;
    std::vector<bool> visited;

public:
    BFSWorkload(const BFSConfig& cfg) : config(cfg) {
        vertex_to_subarray.resize(config.num_vertices);
        adjacency_list.resize(config.num_vertices);
        vertex_levels.resize(config.num_vertices, -1);
        visited.resize(config.num_vertices, false);

        // Partition vertices across subarrays
        for (int v = 0; v < config.num_vertices; v++) {
            vertex_to_subarray[v] = v % config.num_subarrays;
        }

        // Generate random graph edges
        generateGraph();
    }

    void execute() {
        std::cout << "\n=== BFS Workload Execution ===" << std::endl;
        std::cout << "Vertices: " << config.num_vertices << std::endl;
        std::cout << "Avg degree: " << config.avg_degree << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Copy latency: " << config.copy_latency << " cycles" << std::endl;

        // Run BFS from vertex 0
        runBFS(0);

        printMetrics();
    }

private:
    void generateGraph() {
        // Simple random graph generation
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
                }
            }
        }
    }

    void runBFS(int start_vertex) {
        std::queue<int> frontier;
        frontier.push(start_vertex);
        visited[start_vertex] = true;
        vertex_levels[start_vertex] = 0;

        int current_level = 0;

        while (!frontier.empty()) {
            int level_size = frontier.size();

            // Process all vertices at current level
            for (int i = 0; i < level_size; i++) {
                int v = frontier.front();
                frontier.pop();

                int v_subarray = vertex_to_subarray[v];

                // Explore neighbors
                for (int u : adjacency_list[v]) {
                    if (!visited[u]) {
                        visited[u] = true;
                        vertex_levels[u] = current_level + 1;
                        frontier.push(u);

                        int u_subarray = vertex_to_subarray[u];

                        // Check if cross-partition access
                        if (v_subarray != u_subarray) {
                            transferFrontier(v_subarray, u_subarray);
                        } else {
                            // Local access
                            metrics.total_cycles += config.read_latency;
                        }
                    }
                }
            }

            current_level++;
            metrics.num_levels++;
        }
    }

    void transferFrontier(int src_subarray, int dst_subarray) {
        assert(src_subarray != dst_subarray);

        metrics.intersubarray_transfers++;
        metrics.total_cycles += config.copy_latency;

        if (config.copy_latency == 1) {
            metrics.direct_transfers++;
            metrics.total_energy += 0.55;
        } else {
            metrics.htree_traversals++;
            metrics.total_energy += 1.0;
        }
    }

    void printMetrics() {
        std::cout << "\n=== BFS Results ===" << std::endl;
        std::cout << "BFS levels: " << metrics.num_levels << std::endl;
        std::cout << "Total cycles: " << metrics.total_cycles << std::endl;
        std::cout << "Cross-partition edges: " << metrics.cross_partition_edges << std::endl;
        std::cout << "Inter-subarray transfers: " << metrics.intersubarray_transfers << std::endl;

        if (config.copy_latency == 1) {
            std::cout << "Direct transfers (LIBCom): " << metrics.direct_transfers << std::endl;
        } else {
            std::cout << "H-tree traversals (Baseline): " << metrics.htree_traversals << std::endl;
        }

        std::cout << "Total energy (relative): " << metrics.total_energy << std::endl;
        if (metrics.intersubarray_transfers > 0) {
            std::cout << "Avg energy per transfer: "
                      << (metrics.total_energy / metrics.intersubarray_transfers) << std::endl;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <num_subarrays> <num_vertices> <is_libcom>" << std::endl;
        return 1;
    }

    BFSConfig config;
    config.num_subarrays = std::atoi(argv[1]);
    config.num_vertices = std::atoi(argv[2]);
    bool is_libcom = (std::atoi(argv[3]) == 1);

    config.avg_degree = 8;  // Average 8 edges per vertex
    config.subarray_size_words = 1024;

    config.read_latency = 1;
    config.write_latency = 1;

    if (is_libcom) {
        config.copy_latency = 1;
        config.move_latency = 1;
    } else {
        int htree_latency = 0;
        int n = config.num_subarrays;
        while (n > 1) { htree_latency++; n >>= 1; }
        config.copy_latency = 2 + htree_latency;
        config.move_latency = 2 + htree_latency;
    }

    BFSWorkload workload(config);
    workload.execute();

    return 0;
}
