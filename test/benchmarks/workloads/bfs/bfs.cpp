/**
 * @file bfs.cpp
 * @brief Breadth-First Search workload for PIMID
 *
 * This is an external workload binary that can run standalone or under PIMID.
 * It uses OpenMP for parallelization and PIMID API for PIM regions.
 *
 * Usage:
 *   Standalone: ./bfs --vertices 256000 --degree 16
 *   Under PIMID: pimid --config config.yaml --workload ./bfs --vertices 256000 --degree 16
 */

#include <iostream>
#include <vector>
#include <queue>
#include <random>
#include <cstring>
#include <cstdlib>
#include <chrono>

// Always include PIMID API - it provides stubs when not active
#include <pimid/api.h>

#ifdef _OPENMP
#include <omp.h>
#endif

//=============================================================================
// Graph Structure
//=============================================================================

struct Graph {
    uint32_t num_vertices;
    uint32_t avg_degree;
    std::vector<uint32_t> offsets;      // CSR offset array
    std::vector<uint32_t> neighbors;     // CSR neighbor array

    Graph(uint32_t n, uint32_t deg) : num_vertices(n), avg_degree(deg) {
        offsets.resize(n + 1);
        neighbors.reserve(n * deg);
    }
};

//=============================================================================
// Graph Generation
//=============================================================================

void generate_random_graph(Graph& graph) {
    std::cout << "Generating random graph: " << graph.num_vertices
              << " vertices, avg degree " << graph.avg_degree << std::endl;

    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<uint32_t> dist(0, graph.num_vertices - 1);

    graph.offsets[0] = 0;

    for (uint32_t v = 0; v < graph.num_vertices; v++) {
        // Generate random number of neighbors (around avg_degree)
        uint32_t degree = graph.avg_degree;
        if (graph.avg_degree > 2) {
            std::uniform_int_distribution<uint32_t> deg_dist(
                graph.avg_degree / 2,
                graph.avg_degree * 3 / 2
            );
            degree = deg_dist(rng);
        }

        // Add random neighbors
        for (uint32_t i = 0; i < degree; i++) {
            uint32_t neighbor = dist(rng);
            if (neighbor != v) {  // No self-loops
                graph.neighbors.push_back(neighbor);
            }
        }

        graph.offsets[v + 1] = graph.neighbors.size();
    }

    std::cout << "Graph generated: " << graph.neighbors.size() << " edges" << std::endl;
}

//=============================================================================
// BFS Implementation
//=============================================================================

struct BFSResult {
    std::vector<uint32_t> distances;
    uint32_t vertices_visited;
    double execution_time_ns;
};

BFSResult bfs_sequential(const Graph& graph, uint32_t root) {
    BFSResult result;
    result.distances.resize(graph.num_vertices, UINT32_MAX);
    result.vertices_visited = 0;

    auto start = std::chrono::high_resolution_clock::now();

    PIMID_BEGIN_PIM_REGION("BFS_Sequential");

    std::queue<uint32_t> queue;

    // Initialize root
    result.distances[root] = 0;
    queue.push(root);
    result.vertices_visited = 1;

    // BFS traversal
    while (!queue.empty()) {
        uint32_t current = queue.front();
        queue.pop();

        uint32_t start_idx = graph.offsets[current];
        uint32_t end_idx = graph.offsets[current + 1];

        // Process all neighbors
        for (uint32_t i = start_idx; i < end_idx; i++) {
            uint32_t neighbor = graph.neighbors[i];

            // Check if not visited
            if (result.distances[neighbor] == UINT32_MAX) {
                result.distances[neighbor] = result.distances[current] + 1;
                queue.push(neighbor);
                result.vertices_visited++;
            }
        }
    }

    PIMID_END_PIM_REGION();

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    result.execution_time_ns = elapsed.count();

    return result;
}

#ifdef _OPENMP
BFSResult bfs_parallel_openmp(const Graph& graph, uint32_t root) {
    BFSResult result;
    result.distances.resize(graph.num_vertices, UINT32_MAX);
    result.vertices_visited = 0;

    auto start = std::chrono::high_resolution_clock::now();

    PIMID_BEGIN_PIM_REGION("BFS_OpenMP");

    // Level-synchronous BFS
    std::vector<uint32_t> current_frontier;
    std::vector<uint32_t> next_frontier;

    // Initialize root
    result.distances[root] = 0;
    current_frontier.push_back(root);
    result.vertices_visited = 1;

    uint32_t current_level = 0;

    while (!current_frontier.empty()) {
        next_frontier.clear();

        // Process current frontier in parallel
        #pragma omp parallel
        {
            std::vector<uint32_t> local_next;

            #pragma omp for nowait
            for (size_t i = 0; i < current_frontier.size(); i++) {
                uint32_t current = current_frontier[i];
                uint32_t start_idx = graph.offsets[current];
                uint32_t end_idx = graph.offsets[current + 1];

                // Process all neighbors
                for (uint32_t j = start_idx; j < end_idx; j++) {
                    uint32_t neighbor = graph.neighbors[j];

                    // Check if not visited (atomic compare-and-swap)
                    uint32_t expected = UINT32_MAX;
                    if (__sync_bool_compare_and_swap(&result.distances[neighbor],
                                                      expected,
                                                      current_level + 1)) {
                        local_next.push_back(neighbor);
                    }
                }
            }

            // Merge local frontiers
            #pragma omp critical
            {
                next_frontier.insert(next_frontier.end(),
                                    local_next.begin(),
                                    local_next.end());
            }
        }

        // Swap frontiers
        current_frontier.swap(next_frontier);
        current_level++;
    }

    // Count visited vertices
    uint32_t visited_count = 0;
    #pragma omp parallel for reduction(+:visited_count)
    for (size_t i = 0; i < result.distances.size(); i++) {
        if (result.distances[i] != UINT32_MAX) {
            visited_count++;
        }
    }
    result.vertices_visited = visited_count;

    PIMID_END_PIM_REGION();

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    result.execution_time_ns = elapsed.count();

    return result;
}
#endif

//=============================================================================
// Main
//=============================================================================

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --vertices N    Number of vertices (default: 1024)" << std::endl;
    std::cout << "  --degree D      Average degree (default: 16)" << std::endl;
    std::cout << "  --root R        Root vertex for BFS (default: 0)" << std::endl;
    std::cout << "  --parallel      Use OpenMP parallel version" << std::endl;
    std::cout << "  --help          Show this help message" << std::endl;
}

int main(int argc, char** argv) {
    // Parse command line arguments
    uint32_t num_vertices = 1024;
    uint32_t avg_degree = 16;
    uint32_t root = 0;
    bool use_parallel = false;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--vertices") == 0 && i + 1 < argc) {
            num_vertices = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--degree") == 0 && i + 1 < argc) {
            avg_degree = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            root = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--parallel") == 0) {
            use_parallel = true;
        } else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    // Initialize PIMID (no-op if not running under PIMID)
    auto pimid_ctx = pimid_init();

    std::cout << "========================================" << std::endl;
    std::cout << "BFS Workload" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Vertices: " << num_vertices << std::endl;
    std::cout << "Avg Degree: " << avg_degree << std::endl;
    std::cout << "Root: " << root << std::endl;

#ifdef _OPENMP
    std::cout << "OpenMP: Available (" << omp_get_max_threads() << " threads)" << std::endl;
    if (use_parallel) {
        std::cout << "Mode: Parallel (OpenMP)" << std::endl;
    } else {
        std::cout << "Mode: Sequential" << std::endl;
    }
#else
    std::cout << "OpenMP: Not available" << std::endl;
    std::cout << "Mode: Sequential" << std::endl;
    use_parallel = false;
#endif

    if (pimid_is_active()) {
        std::cout << "\nRunning under PIMID:" << std::endl;
        std::cout << "  Memory Tech: " << pimid_get_memory_tech() << std::endl;
        std::cout << "  Placement: " << pimid_get_placement_level() << std::endl;
        std::cout << "  Num PEs: " << pimid_get_num_pes() << std::endl;
    }

    std::cout << "========================================\n" << std::endl;

    // Generate graph
    Graph graph(num_vertices, avg_degree);
    generate_random_graph(graph);

    std::cout << std::endl;

    // Run BFS
    BFSResult result;

#ifdef _OPENMP
    if (use_parallel) {
        result = bfs_parallel_openmp(graph, root);
    } else {
        result = bfs_sequential(graph, root);
    }
#else
    result = bfs_sequential(graph, root);
#endif

    // Print results
    std::cout << "\n========================================" << std::endl;
    std::cout << "BFS Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Vertices visited: " << result.vertices_visited
              << " / " << num_vertices << std::endl;
    std::cout << "Execution time: " << result.execution_time_ns / 1e6 << " ms" << std::endl;

    if (pimid_is_active()) {
        std::cout << "PIMID simulation time: " << pimid_get_time_ns() / 1e6 << " ms" << std::endl;
        std::cout << "PIMID energy: " << pimid_get_energy_pj() / 1e6 << " μJ" << std::endl;
    }

    double throughput = (result.vertices_visited / (result.execution_time_ns / 1e9)) / 1e6;
    std::cout << "Throughput: " << throughput << " M vertices/sec" << std::endl;

    double edges_processed = graph.neighbors.size();
    double edge_throughput = (edges_processed / (result.execution_time_ns / 1e9)) / 1e6;
    std::cout << "Edge throughput: " << edge_throughput << " M edges/sec" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // Finalize PIMID
    pimid_finalize(pimid_ctx);

    return 0;
}

// Workload entry point for dynamic loading by PIMID
#ifdef PIMID_ENABLED
extern "C" int workload_main(int argc, char** argv, pimid_context_t ctx) {
    return main(argc, argv);
}
#endif
