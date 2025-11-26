/**
 * @file pagerank_cosim.cpp
 * @brief COMPLEX Host/Device Co-Simulation: PageRank Algorithm
 *
 * HOST (OOO core with cache):
 *   - Generates web graph (random or power-law)
 *   - Initializes PageRank vector
 *   - Checks convergence (L1 norm of difference)
 *   - Normalizes rank vector
 *   - Displays top-ranked pages
 *
 * DEVICE (ALU cores without cache):
 *   - Computes partial rank contributions in parallel
 *   - Each PE handles a subset of pages
 *   - PageRank update formula with damping factor
 *
 * Collaboration: ITERATIVE host-device cooperation until convergence
 * Complexity: Graph algorithm, iterative convergence, rank propagation
 */

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cmath>
#include <algorithm>

const double DAMPING_FACTOR = 0.85;
const double CONVERGENCE_THRESHOLD = 0.0001;
const int MAX_ITERATIONS = 100;

struct WebGraph {
    int num_pages;
    int num_edges;
    int** outlinks;           // outlinks[i] = array of pages i links to
    int* outlink_counts;      // Number of outlinks for each page
    double* current_ranks;    // Current PageRank values
    double* new_ranks;        // Next iteration ranks
    int num_device_pes;
    double* pe_local_ranks;   // Local rank contributions per PE
};

// ============================================================================
// HOST CODE (runs on OOO/Simple core with cache)
// ============================================================================

class HostPageRankCoordinator {
private:
    WebGraph& graph;
    int iteration;

public:
    HostPageRankCoordinator(WebGraph& web_graph)
        : graph(web_graph), iteration(0) {}

    // HOST: Generate random web graph
    void generateWebGraph() {
        std::cout << "[HOST] Generating web graph with " << graph.num_pages
                  << " pages" << std::endl;

        graph.outlinks = new int*[graph.num_pages];
        graph.outlink_counts = new int[graph.num_pages];
        graph.current_ranks = new double[graph.num_pages];
        graph.new_ranks = new double[graph.num_pages];
        graph.pe_local_ranks = new double[graph.num_pages * graph.num_device_pes];

        graph.num_edges = 0;

        // Generate outlinks for each page
        for (int i = 0; i < graph.num_pages; i++) {
            // Random number of outlinks (3-10)
            int num_outlinks = 3 + (rand() % 8);
            graph.outlink_counts[i] = num_outlinks;
            graph.outlinks[i] = new int[num_outlinks];

            // Random outlinks
            for (int j = 0; j < num_outlinks; j++) {
                graph.outlinks[i][j] = rand() % graph.num_pages;
                graph.num_edges++;
            }
        }

        std::cout << "[HOST] Web graph created:" << std::endl;
        std::cout << "[HOST]   Pages: " << graph.num_pages << std::endl;
        std::cout << "[HOST]   Edges: " << graph.num_edges << std::endl;
        std::cout << "[HOST]   Avg outlinks: " << (double)graph.num_edges / graph.num_pages
                  << std::endl;
    }

    // HOST: Initialize PageRank values
    void initializePageRank() {
        std::cout << "[HOST] Initializing PageRank values (uniform distribution)" << std::endl;

        double initial_rank = 1.0 / graph.num_pages;

        for (int i = 0; i < graph.num_pages; i++) {
            graph.current_ranks[i] = initial_rank;
            graph.new_ranks[i] = 0.0;
        }

        for (int i = 0; i < graph.num_pages * graph.num_device_pes; i++) {
            graph.pe_local_ranks[i] = 0.0;
        }

        std::cout << "[HOST] Initial rank per page: " << initial_rank << std::endl;
    }

    // HOST: Start iteration
    void startIteration() {
        iteration++;
        std::cout << "[HOST] Iteration " << iteration << ": Computing rank propagation" << std::endl;

        // Reset new_ranks and PE-local ranks
        for (int i = 0; i < graph.num_pages; i++) {
            graph.new_ranks[i] = 0.0;
        }

        for (int i = 0; i < graph.num_pages * graph.num_device_pes; i++) {
            graph.pe_local_ranks[i] = 0.0;
        }
    }

    // HOST: Update ranks and check convergence
    bool updateRanksAndCheckConvergence() {
        std::cout << "[HOST] Aggregating PE-local ranks and applying damping..." << std::endl;

        // Aggregate from all PEs
        for (int pe = 0; pe < graph.num_device_pes; pe++) {
            for (int i = 0; i < graph.num_pages; i++) {
                int idx = pe * graph.num_pages + i;
                graph.new_ranks[i] += graph.pe_local_ranks[idx];
            }
        }

        // Apply damping factor: PR(i) = (1-d)/N + d * sum(PR(j)/L(j))
        double base_rank = (1.0 - DAMPING_FACTOR) / graph.num_pages;
        for (int i = 0; i < graph.num_pages; i++) {
            graph.new_ranks[i] = base_rank + DAMPING_FACTOR * graph.new_ranks[i];
        }

        // Normalize (optional, for stability)
        double sum_ranks = 0.0;
        for (int i = 0; i < graph.num_pages; i++) {
            sum_ranks += graph.new_ranks[i];
        }

        // Check convergence (L1 norm of difference)
        double diff = 0.0;
        for (int i = 0; i < graph.num_pages; i++) {
            diff += std::abs(graph.new_ranks[i] - graph.current_ranks[i]);
        }

        // Update current ranks
        for (int i = 0; i < graph.num_pages; i++) {
            graph.current_ranks[i] = graph.new_ranks[i];
        }

        std::cout << "[HOST] Rank sum: " << sum_ranks << ", L1 difference: " << diff << std::endl;

        bool converged = (diff < CONVERGENCE_THRESHOLD);
        if (converged) {
            std::cout << "[HOST] ✓ Converged! (diff < " << CONVERGENCE_THRESHOLD << ")" << std::endl;
        }

        return converged;
    }

    // HOST: Display top-ranked pages
    void displayTopPages() {
        std::cout << "[HOST] PageRank computation complete!" << std::endl;
        std::cout << "[HOST] Total iterations: " << iteration << std::endl;

        // Find top 10 pages
        std::vector<std::pair<int, double>> page_ranks;
        for (int i = 0; i < graph.num_pages; i++) {
            page_ranks.push_back({i, graph.current_ranks[i]});
        }

        std::sort(page_ranks.begin(), page_ranks.end(),
                 [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
                     return a.second > b.second;
                 });

        std::cout << "[HOST] Top 10 ranked pages:" << std::endl;
        int top_k = std::min(10, graph.num_pages);
        for (int i = 0; i < top_k; i++) {
            std::cout << "[HOST]   Rank " << (i + 1) << ": Page "
                      << page_ranks[i].first << " (score: " << page_ranks[i].second
                      << ")" << std::endl;
        }
    }

    // HOST: Cleanup
    void cleanup() {
        std::cout << "[HOST] Cleaning up web graph data" << std::endl;
        for (int i = 0; i < graph.num_pages; i++) {
            delete[] graph.outlinks[i];
        }
        delete[] graph.outlinks;
        delete[] graph.outlink_counts;
        delete[] graph.current_ranks;
        delete[] graph.new_ranks;
        delete[] graph.pe_local_ranks;
    }

    int getIteration() const { return iteration; }
};

// ============================================================================
// DEVICE CODE (runs on ALU cores without cache)
// ============================================================================

class DeviceRankPropagator {
private:
    int pe_id;
    WebGraph& graph;

public:
    DeviceRankPropagator(int id, WebGraph& web_graph)
        : pe_id(id), graph(web_graph) {}

    // DEVICE: Propagate rank contributions
    void propagateRanks() {
        int chunk_size = (graph.num_pages + graph.num_device_pes - 1) / graph.num_device_pes;
        int start = pe_id * chunk_size;
        int end = std::min(start + chunk_size, graph.num_pages);

        // DEVICE: Rank propagation computation
        for (int i = start; i < end; i++) {
            double rank_contribution = graph.current_ranks[i] / graph.outlink_counts[i];

            // Distribute rank to outlinks
            for (int j = 0; j < graph.outlink_counts[i]; j++) {
                int target_page = graph.outlinks[i][j];
                int idx = pe_id * graph.num_pages + target_page;
                graph.pe_local_ranks[idx] += rank_contribution;
            }
        }

        if (pe_id == 0) {
            std::cout << "[DEVICE PE-" << pe_id << "] Propagated ranks from pages "
                      << start << " to " << end << std::endl;
        }
    }
};

// ============================================================================
// MAIN: Orchestrates Host/Device Co-Simulation
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <num_pages> <num_device_pes>" << std::endl;
        return 1;
    }

    int num_pages = std::atoi(argv[1]);
    int num_pes = std::atoi(argv[2]);

    std::cout << "=== HOST/DEVICE CO-SIMULATION: PageRank Algorithm ===\n";
    std::cout << "Web pages: " << num_pages << std::endl;
    std::cout << "Damping factor: " << DAMPING_FACTOR << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    WebGraph web_graph;
    web_graph.num_pages = num_pages;
    web_graph.num_device_pes = num_pes;

    // HOST: Setup
    HostPageRankCoordinator host(web_graph);
    host.generateWebGraph();
    host.initializePageRank();

    // DEVICE: Create PEs
    std::vector<DeviceRankPropagator> device_pes;
    for (int i = 0; i < num_pes; i++) {
        device_pes.emplace_back(i, web_graph);
    }

    std::cout << "--- STARTING ITERATIVE PAGERANK ---\n\n";

    // ITERATIVE HOST-DEVICE COOPERATION
    bool converged = false;
    while (!converged && host.getIteration() < MAX_ITERATIONS) {
        // HOST: Start iteration
        host.startIteration();

        // DEVICE: Propagate ranks
        for (auto& pe : device_pes) {
            pe.propagateRanks();
        }

        // HOST: Update and check convergence
        converged = host.updateRanksAndCheckConvergence();
        std::cout << std::endl;
    }

    std::cout << "--- PAGERANK CONVERGED ---\n\n";

    // HOST: Display results
    host.displayTopPages();
    host.cleanup();

    std::cout << "\n=== Co-Simulation Complete ===\n";
    return 0;
}
