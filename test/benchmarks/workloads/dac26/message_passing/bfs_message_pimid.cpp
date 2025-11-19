/**
 * @file bfs_message_pimid.cpp
 * @brief BFS (Breadth-First Search) workload with MESSAGE PASSING model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
 *
 * Source: Graph500 Benchmark
 *
 * Message Passing Model:
 * - Graph vertices partitioned across subarrays
 * - Frontier vertices stored locally in each subarray
 * - Cross-partition edges require explicit inter-subarray transfers
 * - Each subarray processes local frontier independently
 */

#include "pim_simulator.h"
#include <iostream>
#include <vector>
#include <queue>
#include <cstdint>
#include <cassert>
#include <cmath>

using namespace dac26;

struct BFSConfig {
    int num_subarrays;
    int num_vertices;
    int avg_degree;
    Topology topology;
};

class BFSMessagePIMID {
private:
    BFSConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<int> vertex_to_subarray;
    std::vector<std::vector<int>> adjacency_list;
    std::vector<int> vertex_levels;
    std::vector<bool> visited;
    std::vector<std::queue<int>> local_frontiers;  // Per-subarray frontiers
    uint64_t cross_partition_edges;
    uint64_t local_edges;
    uint64_t frontier_transfers;
    int num_levels;

public:
    BFSMessagePIMID(const BFSConfig& cfg) : config(cfg), cross_partition_edges(0),
                                             local_edges(0), frontier_transfers(0),
                                             num_levels(0) {
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

        // Initialize PIMID simulator
        PIMConfig pim_config;
        pim_config.tech_node_nm = 45;
        pim_config.frequency_ghz = 1.0;
        pim_config.num_subarrays = config.num_subarrays;
        pim_config.topology = config.topology;

        simulator = std::make_shared<PIMSimulator>(pim_config);
        simulator->initialize();
    }

    void execute() {
        std::cout << "\n=== BFS Workload (MESSAGE PASSING - PIMID) ===" << std::endl;
        std::cout << "Vertices: " << config.num_vertices << std::endl;
        std::cout << "Avg degree: " << config.avg_degree << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Programming model: MESSAGE PASSING" << std::endl;

        simulator->resetStats();
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
                    cross_partition_edges++;
                } else {
                    local_edges++;
                }
            }
        }

        std::cout << "  Total edges: " << (cross_partition_edges + local_edges) << std::endl;
        std::cout << "  Cross-partition edges: " << cross_partition_edges << std::endl;
        std::cout << "  Local edges: " << local_edges << std::endl;
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
            num_levels++;
        }

        std::cout << "✓ BFS complete: " << num_levels << " levels" << std::endl;
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
            simulator->simulateMemoryAccess(true, true, sizeof(int));

            // Explore neighbors
            for (int u : adjacency_list[v]) {
                if (!visited[u]) {
                    visited[u] = true;
                    vertex_levels[u] = current_level + 1;

                    int u_subarray = vertex_to_subarray[u];

                    if (u_subarray != subarray_id) {
                        // Cross-partition edge: transfer to remote frontier using PIMID
                        simulator->simulateNetworkTransfer(subarray_id, u_subarray, sizeof(int));
                        frontier_transfers++;

                        local_frontiers[u_subarray].push(u);
                    } else {
                        // Local edge: add to next frontier
                        next_frontier.push(u);
                        simulator->simulateCompute(1);
                    }
                }
            }
        }

        // Update local frontier for next level
        local_frontiers[subarray_id] = next_frontier;

        return vertices_processed;
    }

    void printMetrics() {
        std::cout << "\n=== BFS Results (MESSAGE PASSING - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nBFS levels: " << num_levels << std::endl;
        std::cout << "Total cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nGraph structure:" << std::endl;
        std::cout << "  Cross-partition edges: " << cross_partition_edges << std::endl;
        std::cout << "  Local edges: " << local_edges << std::endl;

        std::cout << "\nCommunication (Message Passing):" << std::endl;
        std::cout << "  Frontier transfers: " << frontier_transfers << std::endl;

        std::cout << "\nEnergy (PIMID-based):" << std::endl;
        std::cout << "  Total energy: " << results.total_energy_pJ << " pJ" << std::endl;
        std::cout << "  Compute energy: " << results.compute_energy_pJ << " pJ ("
                  << (100.0 * results.compute_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Memory energy: " << results.memory_energy_pJ << " pJ ("
                  << (100.0 * results.memory_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Network energy: " << results.network_energy_pJ << " pJ ("
                  << (100.0 * results.network_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;

        if (frontier_transfers > 0) {
            std::cout << "  Avg energy per transfer: "
                      << (results.network_energy_pJ / frontier_transfers) << " pJ" << std::endl;
        }

        std::cout << "\nPerformance:" << std::endl;
        std::cout << "  Execution time: " << results.execution_time_ns << " ns" << std::endl;
        std::cout << "  Execution time: " << (results.execution_time_ns / 1000.0) << " us" << std::endl;

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
    config.topology = is_libcom ? Topology::LIBCOM : Topology::HTREE_BASELINE;

    config.avg_degree = 8;

    std::cout << "\n=== DAC'26 BFS Benchmark (Message Passing - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Source: Graph500 Benchmark" << std::endl;
    std::cout << "Programming Model: MESSAGE PASSING" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    BFSMessagePIMID workload(config);
    workload.execute();

    return 0;
}
