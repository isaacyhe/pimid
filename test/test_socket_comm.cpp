#include "communication/socket_comm.h"
#include "host_engine/host_engine.h"
#include "device_engine/device_engine.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace pimid;

void runHostEngine() {
    PIMIDConfig config;
    config.memory_tech = MemoryTechnology::DRAM;
    config.addressing_mode = AddressingMode::UNIFIED;
    config.pe_placement_level = PEPlacementLevel::BANK;

    HostEngine host(config, 9999);
    
    try {
        host.initialize();
        
        // Send an offload request
        std::cout << "\n[TEST] Host: Sending offload request..." << std::endl;
        host.offloadToDevice(0x1000, 0x2000, 1024);
        
        // Wait for completion
        host.waitForDeviceCompletion();
        
        // Run for a few cycles
        host.run(5000);
        
        host.finalize();
        
        std::cout << "\n[TEST] Host: Test completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[TEST] Host error: " << e.what() << std::endl;
    }
}

void runDeviceEngine() {
    // Give host time to start listening
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    PIMIDConfig config;
    config.memory_tech = MemoryTechnology::DRAM;
    config.addressing_mode = AddressingMode::UNIFIED;
    config.pe_placement_level = PEPlacementLevel::BANK;

    DeviceEngine device(config, "127.0.0.1", 9999);
    
    try {
        device.initialize();
        
        // Run the device simulation
        device.run(5000);
        
        device.finalize();
        
        std::cout << "\n[TEST] Device: Test completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[TEST] Device error: " << e.what() << std::endl;
    }
}

int main(int argc, char** argv) {
    std::cout << "=== Socket Communication Test ===" << std::endl;
    std::cout << "Testing host-device communication via TCP sockets" << std::endl;
    std::cout << "====================================\n" << std::endl;

    // Start host and device in separate threads
    std::thread host_thread(runHostEngine);
    std::thread device_thread(runDeviceEngine);

    // Wait for both to complete
    host_thread.join();
    device_thread.join();

    std::cout << "\n=== Test Complete ===" << std::endl;
    
    return 0;
}
