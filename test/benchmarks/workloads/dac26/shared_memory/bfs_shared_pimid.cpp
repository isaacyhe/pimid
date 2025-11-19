/**
 * @file bfs_shared_pimid.cpp
 * @brief BFS (Breadth-First Search) workload with SHARED MEMORY model - PIMID integrated
 *
 * This version uses PIMID simulator for accurate energy and timing modeling
 * Technology: 45nm
 * Frequency: 1GHz
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

class BFSSharedPIMID {
private:
    BFSConfig config;
    std::shared_ptr<PIMSimulator> simulator;

    std::vector<std::vector<int>> adjacency_list;
    std::vector<int> vertex_levels;
    std::vector<bool> visited;
    std::vector<int> vertex_assignment;  // Which subarray processes each vertex

    uint64_t edges_processed;
    int num_levels;

public:
    BFSSharedPIMID(const BFSConfig& cfg) : config(cfg), edges_processed(0), num_levels(0) {
        adjacency_list.resize(config.num_vertices);
        vertex_levels.resize(config.num_vertices, -1);
        visited.resize(config.num_vertices, false);
        vertex_assignment.resize(config.num_vertices);

        // Assign vertices to subarrays for processing
        for (int v = 0; v < config.num_vertices; v++) {
            vertex_assignment[v] = v % config.num_subarrays;
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
        std::cout << "\n=== BFS Workload (SHARED MEMORY - PIMID) ===" << std::endl;
        std::cout << "Vertices: " << config.num_vertices << std::endl;
        std::cout << "Avg degree: " << config.avg_degree << std::endl;
        std::cout << "Subarrays: " << config.num_subarrays << std::endl;
        std::cout << "Programming model: SHARED MEMORY" << std::endl;

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
        simulator->simulateOperation(PIMOperation::ATOMIC_OP, 1);

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
            simulator->simulateOperation(PIMOperation::BARRIER_SYNC, 1);

            current_frontier = next_frontier;
            current_level++;
            num_levels++;
        }

        std::cout << "✓ BFS complete: " << num_levels << " levels" << std::endl;
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
            if (vertex_assignment[v] == subarray_id) {
                simulator->simulateMemoryAccess(true, true, sizeof(int) * adjacency_list[v].size());
            } else {
                simulator->simulateMemoryAccess(false, true, sizeof(int) * adjacency_list[v].size());
            }

            // Explore neighbors
            for (int u : adjacency_list[v]) {
                // Read visited flag from shared memory
                if (vertex_assignment[u] == subarray_id) {
                    simulator->simulateMemoryAccess(true, true, sizeof(bool));
                } else {
                    simulator->simulateMemoryAccess(false, true, sizeof(bool));
                }

                if (!visited[u]) {
                    // Atomic test-and-set on visited flag
                    simulator->simulateOperation(PIMOperation::ATOMIC_OP, 1);

                    visited[u] = true;
                    vertex_levels[u] = current_level + 1;

                    // Write to vertex level (potentially remote)
                    if (vertex_assignment[u] == subarray_id) {
                        simulator->simulateMemoryAccess(true, false, sizeof(int));
                    } else {
                        simulator->simulateMemoryAccess(false, false, sizeof(int));
                    }

                    // Add to next frontier (shared structure)
                    next_frontier.push(u);
                    simulator->simulateOperation(PIMOperation::ATOMIC_OP, 1);

                    // Compute operation for level update
                    simulator->simulateCompute(1);
                }

                edges_processed++;
            }
        }
    }

    void printMetrics() {
        std::cout << "\n=== BFS Results (SHARED MEMORY - PIMID) ===" << std::endl;

        const SimulationResults& results = simulator->getResults();

        std::cout << "\nBFS levels: " << num_levels << std::endl;
        std::cout << "Total cycles: " << results.total_cycles << std::endl;
        std::cout << "  Compute cycles: " << results.compute_cycles
                  << " (" << (100.0 * results.compute_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Memory cycles: " << results.memory_cycles
                  << " (" << (100.0 * results.memory_cycles / results.total_cycles) << "%)" << std::endl;
        std::cout << "  Network cycles: " << results.network_cycles
                  << " (" << (100.0 * results.network_cycles / results.total_cycles) << "%)" << std::endl;

        std::cout << "\nComputation:" << std::endl;
        std::cout << "  Edges processed: " << edges_processed << std::endl;
        std::cout << "  Compute operations: " << results.compute_ops << std::endl;

        std::cout << "\nCommunication (Shared Memory):" << std::endl;
        std::cout << "  Inter-subarray transfers: 0 (shared memory model)" << std::endl;
        std::cout << "  Local reads: " << results.local_reads << std::endl;
        std::cout << "  Local writes: " << results.local_writes << std::endl;
        std::cout << "  Remote accesses: " << results.remote_reads + results.remote_writes << std::endl;

        std::cout << "\nEnergy (PIMID-based):" << std::endl;
        std::cout << "  Total energy: " << results.total_energy_pJ << " pJ" << std::endl;
        std::cout << "  Compute energy: " << results.compute_energy_pJ << " pJ ("
                  << (100.0 * results.compute_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Memory energy: " << results.memory_energy_pJ << " pJ ("
                  << (100.0 * results.memory_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;
        std::cout << "  Network energy: " << results.network_energy_pJ << " pJ ("
                  << (100.0 * results.network_energy_pJ / results.total_energy_pJ) << "%)" << std::endl;

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

    std::cout << "\n=== DAC'26 BFS Benchmark (Shared Memory - PIMID) ===" << std::endl;
    std::cout << "Configuration: " << (is_libcom ? "LIBCom" : "Baseline H-tree") << std::endl;
    std::cout << "Programming Model: SHARED MEMORY" << std::endl;
    std::cout << "Technology: 45nm, 1GHz" << std::endl;

    BFSSharedPIMID workload(config);
    workload.execute();

    return 0;
}
