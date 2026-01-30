#include "device_engine/device_engine.h"
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
    std::cout << "  --host HOST          Host address to connect to (default: 127.0.0.1)" << std::endl;
    std::cout << "  --port PORT          Port to connect to (default: 9999)" << std::endl;
    std::cout << "  --cycles CYCLES      Number of cycles to simulate (default: 10000)" << std::endl;
    std::cout << "  --delay SECONDS      Delay before connecting (default: 2)" << std::endl;
    std::cout << "  --help               Show this help message" << std::endl;
}

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    int port = 9999;
    uint64_t cycles = 10000;
    int delay = 2;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (arg == "--cycles" && i + 1 < argc) {
            cycles = std::stoull(argv[++i]);
        } else if (arg == "--delay" && i + 1 < argc) {
            delay = std::atoi(argv[++i]);
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "=== PIMID Device Simulator ===" << std::endl;
    std::cout << "Host: " << host << ":" << port << std::endl;
    std::cout << "Simulation cycles: " << cycles << std::endl;
    std::cout << "Connection delay: " << delay << " seconds" << std::endl;
    std::cout << "===============================" << std::endl;

    // Wait before connecting to give host time to start
    if (delay > 0) {
        std::cout << "\n[DEVICE] Waiting " << delay << " seconds before connecting..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(delay));
    }

    // Create configuration
    PIMIDConfig config;
    config.memory_tech = MemoryTechnology::DRAM;
    config.addressing_mode = AddressingMode::UNIFIED;
    config.pe_placement_level = PEPlacementLevel::BANK;

    // Create device engine
    DeviceEngine device(config, host, port);

    try {
        std::cout << "\n[DEVICE] Initializing device engine..." << std::endl;
        device.initialize();
        std::cout << "[DEVICE] Initialization complete" << std::endl;

        std::cout << "[DEVICE] Connected to host at " << host << ":" << port << std::endl;

        // Run simulation - device will process offload requests from host
        std::cout << "\n[DEVICE] Running simulation for " << cycles << " cycles..." << std::endl;
        std::cout << "[DEVICE] Waiting for offload requests from host..." << std::endl;
        device.run(cycles);
        std::cout << "[DEVICE] Simulation complete" << std::endl;

        // Finalize
        std::cout << "\n[DEVICE] Finalizing..." << std::endl;
        device.finalize();

        std::cout << "\n[DEVICE] Device simulator completed successfully!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n[DEVICE ERROR] " << e.what() << std::endl;
        return 1;
    }
}
