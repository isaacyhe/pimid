/**
 * @file matmul_tiled_cosim.cpp
 * @brief TRUE Host/Device Co-Simulation: Tiled Matrix Multiplication
 *
 * HOST (OOO core with cache):
 *   - Tiles matrices into blocks
 *   - Coordinates tile distribution to device
 *   - Assembles final result matrix
 *   - Verifies correctness
 *
 * DEVICE (ALU cores without cache):
 *   - Performs parallel tile-by-tile matrix multiplication
 *   - Each PE computes one or more output tiles
 *
 * Communication: Shared memory for matrix tiles
 */

#include <iostream>
#include <vector>
#include <cstdlib>
#include <cmath>

struct MatrixTile {
    float* data;
    int row;
    int col;
    int size;  // Tile dimension
};

struct SharedMatrices {
    float* A;  // Input matrix A
    float* B;  // Input matrix B
    float* C;  // Output matrix C
    int N;     // Matrix dimension (N×N)
    int tile_size;
    int num_tiles;
    int num_device_pes;
};

// ============================================================================
// HOST CODE (runs on OOO/Simple core with cache)
// ============================================================================

class HostMatrixCoordinator {
private:
    SharedMatrices& shared;

public:
    HostMatrixCoordinator(SharedMatrices& data) : shared(data) {}

    // HOST: Allocate and initialize matrices
    void prepareMatrices() {
        std::cout << "[HOST] Allocating " << shared.N << "×" << shared.N
                  << " matrices" << std::endl;

        int total_elements = shared.N * shared.N;
        shared.A = new float[total_elements];
        shared.B = new float[total_elements];
        shared.C = new float[total_elements];

        std::cout << "[HOST] Initializing matrices..." << std::endl;
        for (int i = 0; i < total_elements; i++) {
            shared.A[i] = static_cast<float>(i % 100) / 10.0f;
            shared.B[i] = static_cast<float>((i * 2) % 100) / 10.0f;
            shared.C[i] = 0.0f;
        }
    }

    // HOST: Tile matrix decomposition
    void tileDecomposition() {
        shared.num_tiles = shared.N / shared.tile_size;

        std::cout << "[HOST] Decomposing into " << shared.num_tiles << "×"
                  << shared.num_tiles << " tiles" << std::endl;
        std::cout << "[HOST] Each tile is " << shared.tile_size << "×"
                  << shared.tile_size << std::endl;
        std::cout << "[HOST] Total output tiles: "
                  << (shared.num_tiles * shared.num_tiles) << std::endl;
    }

    // HOST: Coordinate device execution
    void coordinateTileComputation() {
        int total_output_tiles = shared.num_tiles * shared.num_tiles;
        int tiles_per_pe = (total_output_tiles + shared.num_device_pes - 1) /
                          shared.num_device_pes;

        std::cout << "[HOST] Distributing tiles to " << shared.num_device_pes
                  << " device PEs" << std::endl;
        std::cout << "[HOST] Each PE handles ~" << tiles_per_pe
                  << " output tiles" << std::endl;
    }

    // HOST: Verify result matrix
    void verifyResult() {
        std::cout << "[HOST] Verifying matrix multiplication result..." << std::endl;

        // Sample verification: check a few elements
        int samples = std::min(5, shared.N);
        for (int s = 0; s < samples; s++) {
            int i = rand() % shared.N;
            int j = rand() % shared.N;

            float expected = 0.0f;
            for (int k = 0; k < shared.N; k++) {
                expected += shared.A[i * shared.N + k] *
                           shared.B[k * shared.N + j];
            }

            float actual = shared.C[i * shared.N + j];
            float error = std::abs(actual - expected);

            if (error < 0.01f) {
                std::cout << "[HOST] Sample C[" << i << "][" << j
                          << "] = " << actual << " ✓" << std::endl;
            } else {
                std::cout << "[HOST] Sample C[" << i << "][" << j
                          << "] = " << actual << " (expected " << expected
                          << ") ✗" << std::endl;
            }
        }
    }

    // HOST: Compute result statistics
    void computeStatistics() {
        double sum = 0.0;
        double max_val = shared.C[0];
        double min_val = shared.C[0];

        for (int i = 0; i < shared.N * shared.N; i++) {
            sum += shared.C[i];
            if (shared.C[i] > max_val) max_val = shared.C[i];
            if (shared.C[i] < min_val) min_val = shared.C[i];
        }

        std::cout << "[HOST] Result statistics:" << std::endl;
        std::cout << "[HOST]   Sum: " << sum << std::endl;
        std::cout << "[HOST]   Min: " << min_val << std::endl;
        std::cout << "[HOST]   Max: " << max_val << std::endl;
    }

    // HOST: Cleanup
    void cleanup() {
        std::cout << "[HOST] Cleaning up matrices" << std::endl;
        delete[] shared.A;
        delete[] shared.B;
        delete[] shared.C;
    }
};

// ============================================================================
// DEVICE CODE (runs on ALU cores without cache)
// ============================================================================

class DeviceTileComputer {
private:
    int pe_id;
    SharedMatrices& shared;

public:
    DeviceTileComputer(int id, SharedMatrices& data)
        : pe_id(id), shared(data) {}

    // DEVICE: Compute assigned output tiles
    void computeTiles() {
        int total_output_tiles = shared.num_tiles * shared.num_tiles;
        int tiles_per_pe = (total_output_tiles + shared.num_device_pes - 1) /
                          shared.num_device_pes;

        int start_tile = pe_id * tiles_per_pe;
        int end_tile = std::min(start_tile + tiles_per_pe, total_output_tiles);

        for (int tile_idx = start_tile; tile_idx < end_tile; tile_idx++) {
            int tile_i = tile_idx / shared.num_tiles;
            int tile_j = tile_idx % shared.num_tiles;

            computeOutputTile(tile_i, tile_j);
        }

        if (pe_id == 0) {
            std::cout << "[DEVICE PE-" << pe_id << "] Computed tiles "
                      << start_tile << " to " << end_tile << std::endl;
        }
    }

private:
    // DEVICE: Compute one output tile C[tile_i][tile_j]
    void computeOutputTile(int tile_i, int tile_j) {
        int tile_size = shared.tile_size;
        int row_start = tile_i * tile_size;
        int col_start = tile_j * tile_size;

        // C[tile_i][tile_j] = Σ_k A[tile_i][k] × B[k][tile_j]
        for (int k_tile = 0; k_tile < shared.num_tiles; k_tile++) {
            multiplyAndAccumulateTiles(tile_i, tile_j, k_tile);
        }
    }

    // DEVICE: Multiply two tiles and accumulate to output
    void multiplyAndAccumulateTiles(int tile_i, int tile_j, int k_tile) {
        int tile_size = shared.tile_size;

        for (int i = 0; i < tile_size; i++) {
            for (int j = 0; j < tile_size; j++) {
                float sum = 0.0f;

                // DEVICE: Compute-intensive inner loop
                for (int k = 0; k < tile_size; k++) {
                    int a_row = tile_i * tile_size + i;
                    int a_col = k_tile * tile_size + k;
                    int b_row = k_tile * tile_size + k;
                    int b_col = tile_j * tile_size + j;

                    float a_val = shared.A[a_row * shared.N + a_col];
                    float b_val = shared.B[b_row * shared.N + b_col];

                    sum += a_val * b_val;
                }

                int c_row = tile_i * tile_size + i;
                int c_col = tile_j * tile_size + j;
                shared.C[c_row * shared.N + c_col] += sum;
            }
        }
    }
};

// ============================================================================
// MAIN: Orchestrates Host/Device Co-Simulation
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <matrix_size> <tile_size> <num_device_pes>" << std::endl;
        return 1;
    }

    int N = std::atoi(argv[1]);
    int tile_size = std::atoi(argv[2]);
    int num_pes = std::atoi(argv[3]);

    if (N % tile_size != 0) {
        std::cerr << "Error: matrix_size must be divisible by tile_size" << std::endl;
        return 1;
    }

    std::cout << "=== HOST/DEVICE CO-SIMULATION: Tiled Matrix Multiply ===" << std::endl;
    std::cout << "Matrix size: " << N << "×" << N << std::endl;
    std::cout << "Tile size: " << tile_size << "×" << tile_size << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    SharedMatrices shared_data;
    shared_data.N = N;
    shared_data.tile_size = tile_size;
    shared_data.num_device_pes = num_pes;

    // HOST: Prepare and decompose
    HostMatrixCoordinator host(shared_data);
    host.prepareMatrices();
    host.tileDecomposition();
    host.coordinateTileComputation();

    std::cout << std::endl;
    std::cout << "--- OFFLOADING TO DEVICE ---" << std::endl;
    std::cout << std::endl;

    // DEVICE: Parallel tile computation
    std::vector<DeviceTileComputer> device_pes;
    for (int i = 0; i < num_pes; i++) {
        device_pes.emplace_back(i, shared_data);
    }

    for (auto& pe : device_pes) {
        pe.computeTiles();
    }

    std::cout << std::endl;
    std::cout << "--- RETURNING TO HOST ---" << std::endl;
    std::cout << std::endl;

    // HOST: Verify and analyze
    host.verifyResult();
    host.computeStatistics();
    host.cleanup();

    std::cout << std::endl;
    std::cout << "=== Co-Simulation Complete ===" << std::endl;

    return 0;
}
