/**
 * @file fft_butterfly_cosim.cpp
 * @brief COMPLEX Host/Device Co-Simulation: FFT with Butterfly Operations
 *
 * HOST (OOO core with cache):
 *   - Generates input signal
 *   - Orchestrates multi-stage FFT
 *   - Bit-reversal permutation
 *   - Computes twiddle factors
 *   - Verifies FFT output
 *
 * DEVICE (ALU cores without cache):
 *   - Executes butterfly operations in parallel
 *   - Complex number arithmetic
 *   - Each PE handles a set of butterflies
 *
 * Collaboration: Multi-stage iterative FFT with host orchestration
 * Complexity: Complex arithmetic, multi-stage algorithm, twiddle factors
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <complex>

using Complex = std::complex<double>;

const double PI = 3.14159265358979323846;

struct FFTData {
    int n;                      // FFT size (power of 2)
    Complex* data;              // Input/output data
    Complex* twiddle_factors;   // Pre-computed twiddle factors
    int num_stages;
    int current_stage;
    int num_device_pes;
    int* pe_butterfly_counts;   // Butterflies computed by each PE
};

// ============================================================================
// HOST CODE (runs on OOO/Simple core with cache)
// ============================================================================

class HostFFTOrchestrator {
private:
    FFTData& fft;

public:
    HostFFTOrchestrator(FFTData& fft_data) : fft(fft_data) {}

    // HOST: Generate input signal (sum of sinusoids)
    void generateInputSignal() {
        std::cout << "[HOST] Generating input signal (N=" << fft.n << ")" << std::endl;

        fft.data = new Complex[fft.n];
        fft.pe_butterfly_counts = new int[fft.num_device_pes];

        // Generate sum of 2 sinusoids
        for (int i = 0; i < fft.n; i++) {
            double t = (double)i / fft.n;
            double signal = std::sin(2 * PI * 4 * t) + 0.5 * std::sin(2 * PI * 8 * t);
            fft.data[i] = Complex(signal, 0.0);
        }

        for (int pe = 0; pe < fft.num_device_pes; pe++) {
            fft.pe_butterfly_counts[pe] = 0;
        }

        std::cout << "[HOST] Input signal: sin(2π·4t) + 0.5·sin(2π·8t)" << std::endl;
    }

    // HOST: Bit-reversal permutation
    void bitReversal() {
        std::cout << "[HOST] Performing bit-reversal permutation..." << std::endl;

        int bits = 0;
        int temp_n = fft.n;
        while (temp_n > 1) {
            bits++;
            temp_n >>= 1;
        }

        for (int i = 0; i < fft.n; i++) {
            int reversed = 0;
            for (int b = 0; b < bits; b++) {
                if (i & (1 << b)) {
                    reversed |= (1 << (bits - 1 - b));
                }
            }
            if (reversed > i) {
                std::swap(fft.data[i], fft.data[reversed]);
            }
        }

        std::cout << "[HOST] Bit-reversal complete" << std::endl;
    }

    // HOST: Compute twiddle factors
    void computeTwiddleFactors() {
        std::cout << "[HOST] Pre-computing twiddle factors..." << std::endl;

        fft.twiddle_factors = new Complex[fft.n / 2];

        for (int i = 0; i < fft.n / 2; i++) {
            double angle = -2.0 * PI * i / fft.n;
            fft.twiddle_factors[i] = Complex(std::cos(angle), std::sin(angle));
        }

        // Compute number of stages
        fft.num_stages = 0;
        int temp_n = fft.n;
        while (temp_n > 1) {
            fft.num_stages++;
            temp_n >>= 1;
        }

        std::cout << "[HOST] Twiddle factors computed for " << fft.num_stages
                  << " stages" << std::endl;
    }

    // HOST: Start FFT stage
    void startStage(int stage) {
        fft.current_stage = stage;
        int m = 1 << (stage + 1);  // Group size
        int num_butterflies = fft.n / 2;

        std::cout << "[HOST] Stage " << (stage + 1) << "/" << fft.num_stages
                  << ": " << num_butterflies << " butterflies, group_size=" << m
                  << std::endl;
    }

    // HOST: Verify FFT output
    void verifyOutput() {
        std::cout << "[HOST] Verifying FFT output..." << std::endl;

        // Find magnitude peaks (should be at frequencies 4 and 8)
        std::vector<std::pair<int, double>> peaks;
        for (int i = 0; i < fft.n / 2; i++) {
            double magnitude = std::abs(fft.data[i]);
            if (magnitude > 10.0) {  // Threshold for peak
                peaks.push_back({i, magnitude});
            }
        }

        std::cout << "[HOST] Frequency peaks detected:" << std::endl;
        for (auto& peak : peaks) {
            std::cout << "[HOST]   Bin " << peak.first << ": magnitude "
                      << peak.second << std::endl;
        }

        if (peaks.size() >= 2) {
            std::cout << "[HOST] ✓ Expected frequency components found!" << std::endl;
        }

        // Total butterflies
        int total_butterflies = 0;
        for (int pe = 0; pe < fft.num_device_pes; pe++) {
            total_butterflies += fft.pe_butterfly_counts[pe];
        }
        std::cout << "[HOST] Total butterflies computed: " << total_butterflies << std::endl;
    }

    // HOST: Cleanup
    void cleanup() {
        std::cout << "[HOST] Cleaning up FFT data" << std::endl;
        delete[] fft.data;
        delete[] fft.twiddle_factors;
        delete[] fft.pe_butterfly_counts;
    }
};

// ============================================================================
// DEVICE CODE (runs on ALU cores without cache)
// ============================================================================

class DeviceButterflyComputer {
private:
    int pe_id;
    FFTData& fft;

public:
    DeviceButterflyComputer(int id, FFTData& fft_data)
        : pe_id(id), fft(fft_data) {}

    // DEVICE: Compute butterflies for current stage
    void computeButterflies() {
        int stage = fft.current_stage;
        int m = 1 << (stage + 1);      // Group size
        int m_half = m / 2;
        int num_groups = fft.n / m;

        int total_butterflies = num_groups * m_half;
        int chunk_size = (total_butterflies + fft.num_device_pes - 1) / fft.num_device_pes;
        int start = pe_id * chunk_size;
        int end = std::min(start + chunk_size, total_butterflies);

        // DEVICE: Butterfly computation (complex arithmetic)
        for (int b = start; b < end; b++) {
            int group = b / m_half;
            int butterfly_in_group = b % m_half;

            int i = group * m + butterfly_in_group;
            int j = i + m_half;

            // Twiddle factor index
            int k = butterfly_in_group * (fft.n / m);
            Complex twiddle = fft.twiddle_factors[k];

            // Butterfly operation
            Complex temp = fft.data[j] * twiddle;
            Complex even = fft.data[i];
            Complex odd = temp;

            fft.data[i] = even + odd;
            fft.data[j] = even - odd;

            fft.pe_butterfly_counts[pe_id]++;
        }

        if (pe_id == 0 && total_butterflies > 0) {
            std::cout << "[DEVICE PE-" << pe_id << "] Computed butterflies "
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
                  << " <fft_size_log2> <num_device_pes>" << std::endl;
        std::cerr << "  fft_size_log2: 6-12 (FFT size = 2^N, e.g., 8 → 256-point FFT)" << std::endl;
        return 1;
    }

    int log2_n = std::atoi(argv[1]);
    int num_pes = std::atoi(argv[2]);

    int n = 1 << log2_n;

    std::cout << "=== HOST/DEVICE CO-SIMULATION: FFT with Butterflies ===\n";
    std::cout << "FFT size: " << n << " (2^" << log2_n << ")" << std::endl;
    std::cout << "Device PEs: " << num_pes << std::endl;
    std::cout << std::endl;

    FFTData fft_data;
    fft_data.n = n;
    fft_data.num_device_pes = num_pes;

    // HOST: Setup
    HostFFTOrchestrator host(fft_data);
    host.generateInputSignal();
    host.bitReversal();
    host.computeTwiddleFactors();

    // DEVICE: Create PEs
    std::vector<DeviceButterflyComputer> device_pes;
    for (int i = 0; i < num_pes; i++) {
        device_pes.emplace_back(i, fft_data);
    }

    std::cout << "--- STARTING MULTI-STAGE FFT ---\n\n";

    // MULTI-STAGE FFT
    for (int stage = 0; stage < fft_data.num_stages; stage++) {
        // HOST: Orchestrate stage
        host.startStage(stage);

        // DEVICE: Compute butterflies
        for (auto& pe : device_pes) {
            pe.computeButterflies();
        }
    }

    std::cout << "\n--- FFT COMPLETE ---\n\n";

    // HOST: Verify
    host.verifyOutput();
    host.cleanup();

    std::cout << "\n=== Co-Simulation Complete ===\n";
    return 0;
}
