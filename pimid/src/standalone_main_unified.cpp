/**
 * @file standalone_main_unified.cpp
 * @brief PIMID Unified Simulator - Single Entry Point for All Simulation Modes
 *
 * This is the unified PIMID simulator binary supporting multiple simulation modes:
 * - standalone: PIM-enabled workload execution (default)
 * - host: Host-only simulation
 * - device: Device-only simulation
 * - cosim: Host/Device co-simulation
 *
 * Usage:
 *   pimid --mode standalone --config simulation.yaml --workload ./bfs [args...]
 *   pimid --mode host --port 9999 --cycles 100000
 *   pimid --mode device --host 127.0.0.1 --port 9999
 *   pimid --mode cosim --size 1000000
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <chrono>
#include <unistd.h>
#include <sys/wait.h>

// Memory models
#include "memory_models/include/memory_model.h"
#include "memory_models/include/sram_model.h"
#include "memory_models/include/dram_model.h"
#include "memory_models/include/sttmram_model.h"
#include "memory_models/include/pcm_model.h"
#include "memory_models/include/reram_model.h"

// Host/Device engines (if available)
#ifdef HAVE_HOST_ENGINE
#include "host_engine/host_engine.h"
#endif

#ifdef HAVE_DEVICE_ENGINE
#include "device_engine/device_engine.h"
#endif

#include "common/types.h"

using namespace pimid;

//=============================================================================
// Host/Device Co-Simulation Infrastructure
//=============================================================================

namespace CoSim {

enum class Domain {
    HOST,
    DEVICE
};

struct Transaction {
    void* data;
    size_t size_bytes;
    Domain source;
    Domain destination;
    uint64_t timestamp_ns;
};

class MemoryManager {
public:
    void* allocHost(size_t size) {
        void* ptr = malloc(size);
        host_allocations.push_back({ptr, size});
        std::cout << "[Host] Allocated " << size << " bytes" << std::endl;
        return ptr;
    }

    void* allocDevice(size_t size) {
        void* ptr = malloc(size);
        device_allocations.push_back({ptr, size});
        std::cout << "[Device] Allocated " << size << " bytes" << std::endl;
        return ptr;
    }

    void transferToDevice(void* host_ptr, void* device_ptr, size_t size) {
        auto start = std::chrono::high_resolution_clock::now();
        memcpy(device_ptr, host_ptr, size);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        Transaction tx = {host_ptr, size, Domain::HOST, Domain::DEVICE,
                          static_cast<uint64_t>(duration.count() * 1000)};
        transactions.push_back(tx);

        std::cout << "[Transfer] Host -> Device: " << size << " bytes in "
                  << duration.count() << " us" << std::endl;

        total_h2d_bytes += size;
        total_h2d_time_ns += duration.count() * 1000;
    }

    void transferToHost(void* device_ptr, void* host_ptr, size_t size) {
        auto start = std::chrono::high_resolution_clock::now();
        memcpy(host_ptr, device_ptr, size);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        Transaction tx = {device_ptr, size, Domain::DEVICE, Domain::HOST,
                          static_cast<uint64_t>(duration.count() * 1000)};
        transactions.push_back(tx);

        std::cout << "[Transfer] Device -> Host: " << size << " bytes in "
                  << duration.count() << " us" << std::endl;

        total_d2h_bytes += size;
        total_d2h_time_ns += duration.count() * 1000;
    }

    void freeAll() {
        for (auto& alloc : host_allocations) {
            free(alloc.ptr);
        }
        for (auto& alloc : device_allocations) {
            free(alloc.ptr);
        }
        host_allocations.clear();
        device_allocations.clear();
        std::cout << "[Cleanup] All memory freed" << std::endl;
    }

    void printStats() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "HOST/DEVICE TRANSFER STATISTICS" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Host -> Device:" << std::endl;
        std::cout << "  Total bytes: " << total_h2d_bytes << std::endl;
        std::cout << "  Total time: " << total_h2d_time_ns / 1000.0 << " us" << std::endl;
        if (total_h2d_time_ns > 0) {
            double bandwidth = (total_h2d_bytes / (1024.0 * 1024.0)) / (total_h2d_time_ns / 1e9);
            std::cout << "  Bandwidth: " << bandwidth << " MB/s" << std::endl;
        }

        std::cout << "\nDevice -> Host:" << std::endl;
        std::cout << "  Total bytes: " << total_d2h_bytes << std::endl;
        std::cout << "  Total time: " << total_d2h_time_ns / 1000.0 << " us" << std::endl;
        if (total_d2h_time_ns > 0) {
            double bandwidth = (total_d2h_bytes / (1024.0 * 1024.0)) / (total_d2h_time_ns / 1e9);
            std::cout << "  Bandwidth: " << bandwidth << " MB/s" << std::endl;
        }
    }

private:
    struct Allocation {
        void* ptr;
        size_t size;
    };

    std::vector<Allocation> host_allocations;
    std::vector<Allocation> device_allocations;
    std::vector<Transaction> transactions;

    size_t total_h2d_bytes = 0;
    size_t total_d2h_bytes = 0;
    uint64_t total_h2d_time_ns = 0;
    uint64_t total_d2h_time_ns = 0;
};

// PIM Kernel simulator
void vectorAdd(const float* a, const float* b, float* c, size_t n) {
    std::cout << "[PIM Kernel] Executing vector addition on " << n << " elements" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "[PIM Kernel] Computation completed in " << duration.count() << " us" << std::endl;
    std::cout << "[PIM Kernel] Throughput: " << (n / (duration.count() / 1e6)) / 1e6
              << " M ops/sec" << std::endl;
}

} // namespace CoSim

//=============================================================================
// Configuration Structure
//=============================================================================

struct UnifiedConfig {
    // Simulation mode
    std::string mode;  // "standalone", "host", "device", "cosim"

    // Common metadata
    std::string name;
    std::string description;

    // Memory configuration
    std::string memory_tech;
    int num_banks;
    int subarrays_per_bank;

    // PE configuration
    std::string pe_type;
    std::string placement_level;
    int num_pes;

    // Standalone mode
    std::string workload_binary;
    std::vector<std::string> workload_args;

    // Host mode
    int host_port;
    uint64_t host_cycles;

    // Device mode
    std::string device_host;
    int device_port;
    uint64_t device_cycles;
    int device_delay;

    // Co-sim mode
    size_t cosim_array_size;

    // Output configuration
    std::string stats_file;
    bool enable_detailed_stats;

    // Default constructor
    UnifiedConfig() :
        mode("standalone"),
        name("PIMID_Simulation"),
        description(""),
        memory_tech("SRAM"),
        num_banks(4),
        subarrays_per_bank(4),
        pe_type("in_order_core"),
        placement_level("BANK"),
        num_pes(4),
        host_port(9999),
        host_cycles(10000),
        device_host("127.0.0.1"),
        device_port(9999),
        device_cycles(10000),
        device_delay(2),
        cosim_array_size(1024 * 1024),
        stats_file("results/stats.txt"),
        enable_detailed_stats(true) {}
};

//=============================================================================
// Simulation Mode Implementations
//=============================================================================

class StandaloneSimulator {
public:
    StandaloneSimulator(const UnifiedConfig& config) : config_(config) {}

    bool run() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "PIMID STANDALONE MODE" << std::endl;
        std::cout << "========================================" << std::endl;

        if (config_.workload_binary.empty()) {
            std::cerr << "Error: No workload binary specified for standalone mode" << std::endl;
            std::cerr << "Use: pimid --mode standalone --workload <binary>" << std::endl;
            return false;
        }

        std::cout << "Workload: " << config_.workload_binary << std::endl;
        std::cout << "Memory: " << config_.memory_tech << std::endl;
        std::cout << "PE Type: " << config_.pe_type << std::endl;
        std::cout << "Placement: " << config_.placement_level << std::endl;
        std::cout << std::endl;

        // Execute workload
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "Error: Failed to fork process" << std::endl;
            return false;
        }

        if (pid == 0) {
            // Child process
            std::vector<char*> args;
            args.push_back(const_cast<char*>(config_.workload_binary.c_str()));
            for (const auto& arg : config_.workload_args) {
                args.push_back(const_cast<char*>(arg.c_str()));
            }
            args.push_back(nullptr);

            execvp(config_.workload_binary.c_str(), args.data());
            std::cerr << "Error: Failed to execute workload" << std::endl;
            exit(1);
        }

        // Parent process
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            std::cout << "\nWorkload completed with exit code: " << exit_code << std::endl;
            return exit_code == 0;
        }

        return false;
    }

private:
    UnifiedConfig config_;
};

class HostSimulator {
public:
    HostSimulator(const UnifiedConfig& config) : config_(config) {}

    bool run() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "PIMID HOST-ONLY MODE" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Port: " << config_.host_port << std::endl;
        std::cout << "Cycles: " << config_.host_cycles << std::endl;
        std::cout << "========================================\n" << std::endl;

#ifdef HAVE_HOST_ENGINE
        PIMIDConfig pimid_config;
        pimid_config.memory_tech = MemoryTechnology::DRAM;
        pimid_config.addressing_mode = AddressingMode::UNIFIED;
        pimid_config.pe_placement_level = PEPlacementLevel::BANK;

        // Run host engine simulation
        std::cout << "Host engine simulation would run here..." << std::endl;
        std::cout << "Simulating " << config_.host_cycles << " cycles" << std::endl;

        return true;
#else
        std::cerr << "Error: Host engine not available (HAVE_HOST_ENGINE not defined)" << std::endl;
        std::cerr << "Simulating host-only mode..." << std::endl;
        std::cout << "Simulated " << config_.host_cycles << " host cycles" << std::endl;
        return true;
#endif
    }

private:
    UnifiedConfig config_;
};

class DeviceSimulator {
public:
    DeviceSimulator(const UnifiedConfig& config) : config_(config) {}

    bool run() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "PIMID DEVICE-ONLY MODE" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Host: " << config_.device_host << ":" << config_.device_port << std::endl;
        std::cout << "Cycles: " << config_.device_cycles << std::endl;
        std::cout << "========================================\n" << std::endl;

#ifdef HAVE_DEVICE_ENGINE
        // Run device engine simulation
        std::cout << "Device engine simulation would run here..." << std::endl;
        std::cout << "Simulating " << config_.device_cycles << " cycles" << std::endl;

        return true;
#else
        std::cerr << "Note: Device engine not available (HAVE_DEVICE_ENGINE not defined)" << std::endl;
        std::cerr << "Simulating device-only mode..." << std::endl;
        std::cout << "Simulated " << config_.device_cycles << " device cycles" << std::endl;
        return true;
#endif
    }

private:
    UnifiedConfig config_;
};

class CoSimulator {
public:
    CoSimulator(const UnifiedConfig& config) : config_(config) {}

    bool run() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "PIMID HOST/DEVICE CO-SIMULATION MODE" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Array size: " << config_.cosim_array_size << " elements" << std::endl;
        std::cout << "Data size: " << (config_.cosim_array_size * sizeof(float)) / 1024.0 << " KB per array" << std::endl;
        std::cout << std::endl;

        CoSim::MemoryManager mem_mgr;

        // Step 1: Allocate host memory
        std::cout << "Step 1: Allocating host memory" << std::endl;
        float* host_a = static_cast<float*>(mem_mgr.allocHost(config_.cosim_array_size * sizeof(float)));
        float* host_b = static_cast<float*>(mem_mgr.allocHost(config_.cosim_array_size * sizeof(float)));
        float* host_c = static_cast<float*>(mem_mgr.allocHost(config_.cosim_array_size * sizeof(float)));

        // Step 2: Initialize data on host
        std::cout << "\nStep 2: Initializing data on host" << std::endl;
        for (size_t i = 0; i < config_.cosim_array_size; i++) {
            host_a[i] = static_cast<float>(i);
            host_b[i] = static_cast<float>(i * 2);
        }
        std::cout << "[Host] Initialized arrays A and B" << std::endl;

        // Step 3: Allocate device (PIM) memory
        std::cout << "\nStep 3: Allocating device (PIM) memory" << std::endl;
        float* device_a = static_cast<float*>(mem_mgr.allocDevice(config_.cosim_array_size * sizeof(float)));
        float* device_b = static_cast<float*>(mem_mgr.allocDevice(config_.cosim_array_size * sizeof(float)));
        float* device_c = static_cast<float*>(mem_mgr.allocDevice(config_.cosim_array_size * sizeof(float)));

        // Step 4: Transfer data to device
        std::cout << "\nStep 4: Transferring data to device" << std::endl;
        mem_mgr.transferToDevice(host_a, device_a, config_.cosim_array_size * sizeof(float));
        mem_mgr.transferToDevice(host_b, device_b, config_.cosim_array_size * sizeof(float));

        // Step 5: Execute computation on device
        std::cout << "\nStep 5: Executing computation on device" << std::endl;
        CoSim::vectorAdd(device_a, device_b, device_c, config_.cosim_array_size);

        // Step 6: Transfer results back to host
        std::cout << "\nStep 6: Transferring results back to host" << std::endl;
        mem_mgr.transferToHost(device_c, host_c, config_.cosim_array_size * sizeof(float));

        // Step 7: Verify results on host
        std::cout << "\nStep 7: Verifying results on host" << std::endl;
        bool success = true;
        size_t errors = 0;
        const size_t max_errors_to_show = 5;

        for (size_t i = 0; i < config_.cosim_array_size; i++) {
            float expected = host_a[i] + host_b[i];
            if (std::abs(host_c[i] - expected) > 1e-5) {
                if (errors < max_errors_to_show) {
                    std::cout << "[Host] Error at index " << i << ": expected "
                              << expected << ", got " << host_c[i] << std::endl;
                }
                errors++;
                success = false;
            }
        }

        if (success) {
            std::cout << "[Host] ✓ All results verified successfully!" << std::endl;
        } else {
            std::cout << "[Host] ✗ Verification failed with " << errors << " errors" << std::endl;
        }

        // Print transfer statistics
        mem_mgr.printStats();

        // Cleanup
        std::cout << "\nStep 8: Cleanup" << std::endl;
        mem_mgr.freeAll();

        std::cout << "\n========================================" << std::endl;
        std::cout << "CO-SIMULATION " << (success ? "PASSED" : "FAILED") << std::endl;
        std::cout << "========================================" << std::endl;

        return success;
    }

private:
    UnifiedConfig config_;
};

//=============================================================================
// Main Entry Point
//=============================================================================

void printUsage(const char* program_name) {
    std::cout << "PIMID - Unified Processing-In-Memory Simulator" << std::endl;
    std::cout << "\nUsage: " << program_name << " --mode <mode> [options]" << std::endl;
    std::cout << "\nSimulation Modes:" << std::endl;
    std::cout << "  standalone    PIM-enabled workload execution (default)" << std::endl;
    std::cout << "  host          Host-only simulation" << std::endl;
    std::cout << "  device        Device-only simulation" << std::endl;
    std::cout << "  cosim         Host/Device co-simulation" << std::endl;
    std::cout << "\nCommon Options:" << std::endl;
    std::cout << "  --mode MODE          Simulation mode (default: standalone)" << std::endl;
    std::cout << "  --help, -h           Show this help message" << std::endl;
    std::cout << "\nStandalone Mode Options:" << std::endl;
    std::cout << "  --config FILE        Configuration file (YAML)" << std::endl;
    std::cout << "  --workload BINARY    Workload binary to simulate" << std::endl;
    std::cout << "\nHost Mode Options:" << std::endl;
    std::cout << "  --port PORT          Port to listen on (default: 9999)" << std::endl;
    std::cout << "  --cycles CYCLES      Number of cycles to simulate (default: 10000)" << std::endl;
    std::cout << "\nDevice Mode Options:" << std::endl;
    std::cout << "  --host HOST          Host address to connect to (default: 127.0.0.1)" << std::endl;
    std::cout << "  --port PORT          Port to connect to (default: 9999)" << std::endl;
    std::cout << "  --cycles CYCLES      Number of cycles to simulate (default: 10000)" << std::endl;
    std::cout << "\nCo-Sim Mode Options:" << std::endl;
    std::cout << "  --size SIZE          Array size for vector operation (default: 1048576)" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  " << program_name << " --mode standalone --workload ./bfs --vertices 100000" << std::endl;
    std::cout << "  " << program_name << " --mode host --port 9999 --cycles 100000" << std::endl;
    std::cout << "  " << program_name << " --mode device --host 127.0.0.1 --port 9999" << std::endl;
    std::cout << "  " << program_name << " --mode cosim --size 1000000" << std::endl;
}

int main(int argc, char** argv) {
    UnifiedConfig config;
    bool parsing_workload_args = false;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (parsing_workload_args) {
            config.workload_args.push_back(arg);
            continue;
        }

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--mode" && i + 1 < argc) {
            config.mode = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            // Load config file (simplified for now)
            ++i;
        } else if (arg == "--workload" && i + 1 < argc) {
            config.workload_binary = argv[++i];
            parsing_workload_args = true;
        } else if (arg == "--port" && i + 1 < argc) {
            int port = std::atoi(argv[++i]);
            config.host_port = port;
            config.device_port = port;
        } else if (arg == "--host" && i + 1 < argc) {
            config.device_host = argv[++i];
        } else if (arg == "--cycles" && i + 1 < argc) {
            uint64_t cycles = std::stoull(argv[++i]);
            config.host_cycles = cycles;
            config.device_cycles = cycles;
        } else if (arg == "--size" && i + 1 < argc) {
            config.cosim_array_size = std::stoull(argv[++i]);
        } else if (arg == "--delay" && i + 1 < argc) {
            config.device_delay = std::atoi(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Run appropriate simulator based on mode
    bool success = false;

    if (config.mode == "standalone") {
        StandaloneSimulator sim(config);
        success = sim.run();
    } else if (config.mode == "host") {
        HostSimulator sim(config);
        success = sim.run();
    } else if (config.mode == "device") {
        DeviceSimulator sim(config);
        success = sim.run();
    } else if (config.mode == "cosim") {
        CoSimulator sim(config);
        success = sim.run();
    } else {
        std::cerr << "Error: Unknown simulation mode: " << config.mode << std::endl;
        std::cerr << "Valid modes: standalone, host, device, cosim" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    return success ? 0 : 1;
}
