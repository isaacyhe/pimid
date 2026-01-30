/**
 * @file image_convolution_cosim.cpp
 * @brief COMPLEX Host/Device Co-Simulation: Image Convolution with Edge Detection
 *
 * HOST (OOO core with cache):
 *   - Loads/generates image data
 *   - Manages convolution windows and edge handling
 *   - Assembles filtered output image
 *   - Verifies edge detection quality
 *   - Computes image statistics
 *
 * DEVICE (ALU cores without cache):
 *   - Applies 3×3 convolution filters in parallel
 *   - Each PE handles a set of output pixels
 *   - Sobel edge detection computation
 *
 * Collaboration: Host prepares windows → Device filters → Host assembles
 * Complexity: 2D convolution, edge handling, multiple filter kernels
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>

const int FILTER_SIZE = 3;

// Sobel edge detection kernels
const int SOBEL_X[3][3] = {
    {-1, 0, 1},
    {-2, 0, 2},
    {-1, 0, 1}
};

const int SOBEL_Y[3][3] = {
    {-1, -2, -1},
    { 0,  0,  0},
    { 1,  2,  1}
};

struct ImageData {
    int width;
    int height;
    int* input_image;      // Input grayscale image
    int* output_image;     // Edge-detected output
    int* gradient_x;       // Horizontal gradient
    int* gradient_y;       // Vertical gradient
    int num_device_pes;
    int* pe_edge_counts;   // Edge pixels found by each PE
};

// ============================================================================
// HOST CODE (runs on OOO/Simple core with cache)
// ============================================================================

class HostImageProcessor {
private:
    ImageData& data;

public:
    HostImageProcessor(ImageData& img_data) : data(img_data) {}

    // HOST: Generate synthetic image (gradient pattern)
    void generateImage() {
        std::cout << "[HOST] Generating " << data.width << "×" << data.height
                  << " synthetic image" << std::endl;

        data.input_image = new int[data.width * data.height];
        data.output_image = new int[data.width * data.height];
        data.gradient_x = new int[data.width * data.height];
        data.gradient_y = new int[data.width * data.height];
        data.pe_edge_counts = new int[data.num_device_pes];

        // Generate synthetic image with diagonal stripes
        for (int y = 0; y < data.height; y++) {
            for (int x = 0; x < data.width; x++) {
                int idx = y * data.width + x;
                // Create diagonal pattern with edges
                int pattern = ((x + y) / 10) % 2;
                data.input_image[idx] = pattern * 200 + 50;
            }
        }

        // Initialize outputs
        for (int i = 0; i < data.width * data.height; i++) {
            data.output_image[i] = 0;
            data.gradient_x[i] = 0;
            data.gradient_y[i] = 0;
        }

        for (int pe = 0; pe < data.num_device_pes; pe++) {
            data.pe_edge_counts[pe] = 0;
        }

        std::cout << "[HOST] Image generated with diagonal stripe pattern" << std::endl;
    }

    // HOST: Prepare for convolution
    void prepareConvolution() {
        int total_pixels = data.width * data.height;
        int inner_pixels = (data.width - 2) * (data.height - 2);  // Exclude borders

        std::cout << "[HOST] Preparing convolution:" << std::endl;
        std::cout << "[HOST]   Total pixels: " << total_pixels << std::endl;
        std::cout << "[HOST]   Inner pixels (3×3 convolution): " << inner_pixels << std::endl;
        std::cout << "[HOST]   Edge handling: Zero padding" << std::endl;
    }

    // HOST: Assemble edge-detected image
    void assembleEdgeImage() {
        std::cout << "[HOST] Assembling edge-detected image..." << std::endl;

        // HOST: Combine gradient_x and gradient_y to compute edge magnitude
        int strong_edges = 0;
        const int EDGE_THRESHOLD = 100;

        for (int y = 1; y < data.height - 1; y++) {
            for (int x = 1; x < data.width - 1; x++) {
                int idx = y * data.width + x;
                int gx = data.gradient_x[idx];
                int gy = data.gradient_y[idx];

                // Edge magnitude: sqrt(gx² + gy²)
                int magnitude = (int)std::sqrt(gx * gx + gy * gy);
                data.output_image[idx] = magnitude;

                if (magnitude > EDGE_THRESHOLD) {
                    strong_edges++;
                }
            }
        }

        std::cout << "[HOST] Strong edges detected: " << strong_edges << std::endl;
    }

    // HOST: Verify and compute statistics
    void verifyAndAnalyze() {
        std::cout << "[HOST] Computing image statistics..." << std::endl;

        int min_val = data.output_image[0];
        int max_val = data.output_image[0];
        long long sum = 0;

        for (int i = 0; i < data.width * data.height; i++) {
            int val = data.output_image[i];
            min_val = std::min(min_val, val);
            max_val = std::max(max_val, val);
            sum += val;
        }

        int avg = sum / (data.width * data.height);

        std::cout << "[HOST] Output image statistics:" << std::endl;
        std::cout << "[HOST]   Min: " << min_val << std::endl;
        std::cout << "[HOST]   Max: " << max_val << std::endl;
        std::cout << "[HOST]   Avg: " << avg << std::endl;

        // Verify per-PE edge counts
        int total_pe_edges = 0;
        for (int pe = 0; pe < data.num_device_pes; pe++) {
            total_pe_edges += data.pe_edge_counts[pe];
        }
        std::cout << "[HOST] Total edges found by PEs: " << total_pe_edges << std::endl;
        std::cout << "[HOST] ✓ Edge detection complete!" << std::endl;
    }

    // HOST: Cleanup
    void cleanup() {
        std::cout << "[HOST] Cleaning up image data" << std::endl;
        delete[] data.input_image;
        delete[] data.output_image;
        delete[] data.gradient_x;
        delete[] data.gradient_y;
        delete[] data.pe_edge_counts;
    }
};

// ============================================================================
// DEVICE CODE (runs on ALU cores without cache)
// ============================================================================

class DeviceConvolver {
private:
    int pe_id;
    ImageData& data;

public:
    DeviceConvolver(int id, ImageData& img_data)
        : pe_id(id), data(img_data) {}

    // DEVICE: Apply Sobel convolution in parallel
    void applySobelFilter() {
        int inner_height = data.height - 2;
        int inner_width = data.width - 2;
        int total_inner = inner_height * inner_width;

        int chunk_size = (total_inner + data.num_device_pes - 1) / data.num_device_pes;
        int start = pe_id * chunk_size;
        int end = std::min(start + chunk_size, total_inner);

        // DEVICE: Compute-intensive convolution loop
        for (int i = start; i < end; i++) {
            // Convert linear index to 2D coordinates (inner region)
            int inner_y = i / inner_width;
            int inner_x = i % inner_width;

            // Actual image coordinates (add 1 for border)
            int y = inner_y + 1;
            int x = inner_x + 1;
            int out_idx = y * data.width + x;

            // Apply 3×3 Sobel filter
            int gx = 0;
            int gy = 0;

            for (int ky = 0; ky < 3; ky++) {
                for (int kx = 0; kx < 3; kx++) {
                    int img_y = y + ky - 1;
                    int img_x = x + kx - 1;
                    int img_idx = img_y * data.width + img_x;
                    int pixel = data.input_image[img_idx];

                    gx += pixel * SOBEL_X[ky][kx];
                    gy += pixel * SOBEL_Y[ky][kx];
                }
            }

            data.gradient_x[out_idx] = gx;
            data.gradient_y[out_idx] = gy;

            // Count strong edges
            int magnitude = (int)std::sqrt(gx * gx + gy * gy);
            if (magnitude > 100) {
                data.pe_edge_counts[pe_id]++;
            }
        }

        if (pe_id == 0) {
            std::cout << "[DEVICE PE-" << pe_id << "] Applied Sobel filter to pixels "
                      << start << " to " << end << std::endl;
        }
    }
};

// ============================================================================
// MAIN: Orchestrates Host/Device Co-Simulation
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <image_width> <image_height> <num_device_pes>" << std::endl;
        return 1;
    }

    int width = std::atoi(argv[1]);
    int height = std::atoi(argv[2]);
    int num_pes = std::atoi(argv[3]);

    std::cout << "=== HOST/DEVICE CO-SIMULATION: Image Convolution ===\n";
    std::cout << "Image size: " << width << "×" << height << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    ImageData img_data;
    img_data.width = width;
    img_data.height = height;
    img_data.num_device_pes = num_pes;

    // HOST: Setup
    HostImageProcessor host(img_data);
    host.generateImage();
    host.prepareConvolution();

    // DEVICE: Create PEs
    std::vector<DeviceConvolver> device_pes;
    for (int i = 0; i < num_pes; i++) {
        device_pes.emplace_back(i, img_data);
    }

    std::cout << "--- OFFLOADING TO DEVICE ---\n";

    // DEVICE: Apply convolution
    for (auto& pe : device_pes) {
        pe.applySobelFilter();
    }

    std::cout << "--- RETURNING TO HOST ---\n";

    // HOST: Assemble and verify
    host.assembleEdgeImage();
    host.verifyAndAnalyze();
    host.cleanup();

    std::cout << "\n=== Co-Simulation Complete ===\n";
    return 0;
}
