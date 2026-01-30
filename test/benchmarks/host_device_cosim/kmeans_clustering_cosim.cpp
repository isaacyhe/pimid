/**
 * @file kmeans_clustering_cosim.cpp
 * @brief COMPLEX Host/Device Co-Simulation: K-Means Clustering
 *
 * HOST (OOO core with cache):
 *   - Generates/loads data points
 *   - Initializes cluster centroids
 *   - Updates centroids after each iteration
 *   - Checks convergence (centroid movement threshold)
 *   - Displays final clustering results
 *
 * DEVICE (ALU cores without cache):
 *   - Assigns points to nearest cluster (parallel)
 *   - Computes local centroid contributions
 *   - Euclidean distance calculations
 *
 * Collaboration: ITERATIVE host-device cooperation until convergence
 * Complexity: Iterative convergence, distance computation, centroid updates
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <limits>

const int DIMENSIONS = 2;  // 2D points for visualization
const double CONVERGENCE_THRESHOLD = 0.01;
const int MAX_ITERATIONS = 50;

struct Point {
    double coords[DIMENSIONS];
    int cluster_id;
};

struct Centroid {
    double coords[DIMENSIONS];
};

struct KMeansData {
    int num_points;
    int num_clusters;
    Point* points;
    Centroid* centroids;
    Centroid* new_centroids;     // Temporary for updates
    int* cluster_sizes;          // Points per cluster
    int num_device_pes;
    int* pe_local_sizes;         // Local cluster sizes per PE
    double** pe_local_sums;      // Local centroid sums per PE
};

// ============================================================================
// HOST CODE (runs on OOO/Simple core with cache)
// ============================================================================

class HostClusteringCoordinator {
private:
    KMeansData& data;
    int iteration;

public:
    HostClusteringCoordinator(KMeansData& kmeans_data)
        : data(kmeans_data), iteration(0) {}

    // HOST: Generate random data points
    void generateDataPoints() {
        std::cout << "[HOST] Generating " << data.num_points << " data points" << std::endl;

        data.points = new Point[data.num_points];

        // Generate points in clusters (for realistic data)
        for (int i = 0; i < data.num_points; i++) {
            int true_cluster = i % data.num_clusters;
            for (int d = 0; d < DIMENSIONS; d++) {
                double cluster_center = true_cluster * 10.0;
                double noise = ((double)rand() / RAND_MAX - 0.5) * 3.0;
                data.points[i].coords[d] = cluster_center + noise;
            }
            data.points[i].cluster_id = -1;  // Unassigned
        }

        std::cout << "[HOST] Data points generated in " << DIMENSIONS << "D space" << std::endl;
    }

    // HOST: Initialize centroids (K-means++)
    void initializeCentroids() {
        std::cout << "[HOST] Initializing " << data.num_clusters << " centroids" << std::endl;

        data.centroids = new Centroid[data.num_clusters];
        data.new_centroids = new Centroid[data.num_clusters];
        data.cluster_sizes = new int[data.num_clusters];

        // Simple initialization: first K points
        for (int k = 0; k < data.num_clusters; k++) {
            for (int d = 0; d < DIMENSIONS; d++) {
                data.centroids[k].coords[d] = data.points[k].coords[d];
            }
        }

        // Allocate PE-local arrays
        data.pe_local_sizes = new int[data.num_device_pes * data.num_clusters];
        data.pe_local_sums = new double*[data.num_device_pes * data.num_clusters];
        for (int i = 0; i < data.num_device_pes * data.num_clusters; i++) {
            data.pe_local_sums[i] = new double[DIMENSIONS];
            for (int d = 0; d < DIMENSIONS; d++) {
                data.pe_local_sums[i][d] = 0.0;
            }
        }

        std::cout << "[HOST] Initial centroids set" << std::endl;
    }

    // HOST: Start iteration
    void startIteration() {
        iteration++;
        std::cout << "[HOST] Iteration " << iteration << ": Assigning points to clusters" << std::endl;

        // Reset cluster sizes and sums
        for (int k = 0; k < data.num_clusters; k++) {
            data.cluster_sizes[k] = 0;
            for (int d = 0; d < DIMENSIONS; d++) {
                data.new_centroids[k].coords[d] = 0.0;
            }
        }

        for (int i = 0; i < data.num_device_pes * data.num_clusters; i++) {
            data.pe_local_sizes[i] = 0;
            for (int d = 0; d < DIMENSIONS; d++) {
                data.pe_local_sums[i][d] = 0.0;
            }
        }
    }

    // HOST: Update centroids after device assignment
    bool updateCentroids() {
        std::cout << "[HOST] Aggregating PE-local results and updating centroids..." << std::endl;

        // Aggregate from all PEs
        for (int pe = 0; pe < data.num_device_pes; pe++) {
            for (int k = 0; k < data.num_clusters; k++) {
                int idx = pe * data.num_clusters + k;
                data.cluster_sizes[k] += data.pe_local_sizes[idx];
                for (int d = 0; d < DIMENSIONS; d++) {
                    data.new_centroids[k].coords[d] += data.pe_local_sums[idx][d];
                }
            }
        }

        // Compute new centroids
        for (int k = 0; k < data.num_clusters; k++) {
            if (data.cluster_sizes[k] > 0) {
                for (int d = 0; d < DIMENSIONS; d++) {
                    data.new_centroids[k].coords[d] /= data.cluster_sizes[k];
                }
            }
        }

        // Check convergence
        double max_movement = 0.0;
        for (int k = 0; k < data.num_clusters; k++) {
            double movement = 0.0;
            for (int d = 0; d < DIMENSIONS; d++) {
                double diff = data.new_centroids[k].coords[d] - data.centroids[k].coords[d];
                movement += diff * diff;
            }
            movement = std::sqrt(movement);
            max_movement = std::max(max_movement, movement);
        }

        // Update centroids
        for (int k = 0; k < data.num_clusters; k++) {
            for (int d = 0; d < DIMENSIONS; d++) {
                data.centroids[k].coords[d] = data.new_centroids[k].coords[d];
            }
        }

        std::cout << "[HOST] Max centroid movement: " << max_movement << std::endl;

        bool converged = (max_movement < CONVERGENCE_THRESHOLD);
        if (converged) {
            std::cout << "[HOST] ✓ Converged! (movement < " << CONVERGENCE_THRESHOLD << ")" << std::endl;
        }

        return converged;
    }

    // HOST: Display results
    void displayResults() {
        std::cout << "[HOST] K-Means clustering complete!" << std::endl;
        std::cout << "[HOST] Total iterations: " << iteration << std::endl;
        std::cout << "[HOST] Final cluster sizes:" << std::endl;

        for (int k = 0; k < data.num_clusters; k++) {
            std::cout << "[HOST]   Cluster " << k << ": " << data.cluster_sizes[k]
                      << " points, centroid=(";
            for (int d = 0; d < DIMENSIONS; d++) {
                std::cout << data.centroids[k].coords[d];
                if (d < DIMENSIONS - 1) std::cout << ", ";
            }
            std::cout << ")" << std::endl;
        }
    }

    // HOST: Cleanup
    void cleanup() {
        std::cout << "[HOST] Cleaning up clustering data" << std::endl;
        delete[] data.points;
        delete[] data.centroids;
        delete[] data.new_centroids;
        delete[] data.cluster_sizes;
        delete[] data.pe_local_sizes;
        for (int i = 0; i < data.num_device_pes * data.num_clusters; i++) {
            delete[] data.pe_local_sums[i];
        }
        delete[] data.pe_local_sums;
    }

    int getIteration() const { return iteration; }
};

// ============================================================================
// DEVICE CODE (runs on ALU cores without cache)
// ============================================================================

class DeviceClusterAssigner {
private:
    int pe_id;
    KMeansData& data;

public:
    DeviceClusterAssigner(int id, KMeansData& kmeans_data)
        : pe_id(id), data(kmeans_data) {}

    // DEVICE: Assign points to nearest cluster
    void assignPointsToClusters() {
        int chunk_size = (data.num_points + data.num_device_pes - 1) / data.num_device_pes;
        int start = pe_id * chunk_size;
        int end = std::min(start + chunk_size, data.num_points);

        // DEVICE: Compute-intensive distance calculations
        for (int i = start; i < end; i++) {
            double min_distance = std::numeric_limits<double>::max();
            int nearest_cluster = -1;

            // Find nearest centroid
            for (int k = 0; k < data.num_clusters; k++) {
                double distance = 0.0;
                for (int d = 0; d < DIMENSIONS; d++) {
                    double diff = data.points[i].coords[d] - data.centroids[k].coords[d];
                    distance += diff * diff;
                }
                distance = std::sqrt(distance);

                if (distance < min_distance) {
                    min_distance = distance;
                    nearest_cluster = k;
                }
            }

            // Assign point to cluster
            data.points[i].cluster_id = nearest_cluster;

            // Update local centroid contribution
            int idx = pe_id * data.num_clusters + nearest_cluster;
            data.pe_local_sizes[idx]++;
            for (int d = 0; d < DIMENSIONS; d++) {
                data.pe_local_sums[idx][d] += data.points[i].coords[d];
            }
        }

        if (pe_id == 0) {
            std::cout << "[DEVICE PE-" << pe_id << "] Assigned points " << start
                      << " to " << end << std::endl;
        }
    }
};

// ============================================================================
// MAIN: Orchestrates Host/Device Co-Simulation
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <num_points> <num_clusters> <num_device_pes>" << std::endl;
        return 1;
    }

    int num_points = std::atoi(argv[1]);
    int num_clusters = std::atoi(argv[2]);
    int num_pes = std::atoi(argv[3]);

    std::cout << "=== HOST/DEVICE CO-SIMULATION: K-Means Clustering ===\n";
    std::cout << "Data points: " << num_points << std::endl;
    std::cout << "Clusters: " << num_clusters << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    KMeansData kmeans_data;
    kmeans_data.num_points = num_points;
    kmeans_data.num_clusters = num_clusters;
    kmeans_data.num_device_pes = num_pes;

    // HOST: Setup
    HostClusteringCoordinator host(kmeans_data);
    host.generateDataPoints();
    host.initializeCentroids();

    // DEVICE: Create PEs
    std::vector<DeviceClusterAssigner> device_pes;
    for (int i = 0; i < num_pes; i++) {
        device_pes.emplace_back(i, kmeans_data);
    }

    std::cout << "--- STARTING ITERATIVE K-MEANS ---\n\n";

    // ITERATIVE HOST-DEVICE COOPERATION
    bool converged = false;
    while (!converged && host.getIteration() < MAX_ITERATIONS) {
        // HOST: Start iteration
        host.startIteration();

        // DEVICE: Assign points
        for (auto& pe : device_pes) {
            pe.assignPointsToClusters();
        }

        // HOST: Update centroids and check convergence
        converged = host.updateCentroids();
        std::cout << std::endl;
    }

    std::cout << "--- K-MEANS CONVERGED ---\n\n";

    // HOST: Display results
    host.displayResults();
    host.cleanup();

    std::cout << "\n=== Co-Simulation Complete ===\n";
    return 0;
}
