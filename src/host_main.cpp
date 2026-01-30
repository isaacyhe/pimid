#include "host_engine/host_engine.h"
#include "common/types.h"
#include <iostream>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>

using namespace pimid;

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --port PORT          Port to listen on (default: 9999)" << std::endl;
    std::cout << "  --cycles CYCLES      Number of cycles to simulate (default: 10000)" << std::endl;
    std::cout << "  --help               Show this help message" << std::endl;
}

int main(int argc, char** argv) {
    int port = 9999;
    uint64_t cycles = 10000;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (arg == "--cycles" && i + 1 < argc) {
            cycles = std::stoull(argv[++i]);
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "=== PIMID Host Simulator ===" << std::endl;
    std::cout << "Port: " << port << std::endl;
    std::cout << "Simulation cycles: " << cycles << std::endl;
    std::cout << "=============================" << std::endl;

    // Create configuration
    PIMIDConfig config;
    config.memory_tech = MemoryTechnology::DRAM;
    config.addressing_mode = AddressingMode::UNIFIED;
    config.pe_placement_level = PEPlacementLevel::BANK;

    // Create host engine
    HostEngine host(config, port);

    try {
        std::cout << "\n[HOST] Initializing host engine..." << std::endl;
        host.initialize();
        std::cout << "[HOST] Initialization complete" << std::endl;

        // Wait a moment for connection to establish
        std::cout << "[HOST] Waiting for device connection..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Send some offload requests
        std::cout << "\n[HOST] Sending offload request 1..." << std::endl;
        host.offloadToDevice(0x1000, 0x2000, 1024);

        std::cout << "[HOST] Sending offload request 2..." << std::endl;
        host.offloadToDevice(0x3000, 0x4000, 2048);

        // Wait for completion
        std::cout << "[HOST] Waiting for device completion..." << std::endl;
        host.waitForDeviceCompletion();
        std::cout << "[HOST] Device operations completed" << std::endl;

        // Run simulation
        std::cout << "\n[HOST] Running simulation for " << cycles << " cycles..." << std::endl;
        host.run(cycles);
        std::cout << "[HOST] Simulation complete" << std::endl;

        // Finalize
        std::cout << "\n[HOST] Finalizing..." << std::endl;
        host.finalize();

        std::cout << "\n[HOST] Host simulator completed successfully!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n[HOST ERROR] " << e.what() << std::endl;
        return 1;
    }
}
