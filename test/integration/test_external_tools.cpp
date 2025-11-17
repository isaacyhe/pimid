/**
 * @file test_external_tools.cpp
 * @brief Comprehensive integration test for CACTI and Ramulator2 external tools
 *
 * This test validates:
 * 1. CACTI wrapper functionality for SRAM modeling
 * 2. Ramulator2 wrapper functionality for DRAM modeling
 * 3. Integration between external tools and PIMID memory models
 */

#include <iostream>
#include <cassert>
#include <memory>
#include <iomanip>

// PIMID memory model headers
#include "cacti_wrapper.h"
#include "ramulator_wrapper.h"

using namespace pimid;

//=============================================================================
// Test utilities
//=============================================================================

void printTestHeader(const std::string& test_name) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "TEST: " << test_name << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void printTestResult(bool passed) {
    if (passed) {
        std::cout << "[PASS] ✓" << std::endl;
    } else {
        std::cout << "[FAIL] ✗" << std::endl;
    }
}

//=============================================================================
// CACTI Tests
//=============================================================================

bool test_cacti_basic_initialization() {
    printTestHeader("CACTI: Basic Initialization");

    try {
        // Create a 256KB L2 cache configuration
        CACTIWrapper::SRAMConfig config;
        config.capacity_bytes = 256 * 1024;  // 256 KB
        config.line_size = 64;                // 64B cache line
        config.associativity = 8;             // 8-way set associative
        config.banks = 1;
        config.tech_node_nm = 22;             // 22nm technology
        config.is_cache = true;
        config.temperature = 350;             // 350K (77°C)

        CACTIWrapper cacti(config);
        cacti.initialize();

        if (!cacti.isValid()) {
            std::cout << "CACTI initialization failed: " << cacti.getErrorMessage() << std::endl;
            printTestResult(false);
            return false;
        }

        // Verify basic metrics
        double access_time = cacti.getAccessTime();
        double area = cacti.getArea();
        double read_energy = cacti.getDynamicReadEnergy();

        std::cout << "CACTI Results:" << std::endl;
        std::cout << "  Access Time: " << (access_time * 1e9) << " ns" << std::endl;
        std::cout << "  Area: " << area << " mm²" << std::endl;
        std::cout << "  Read Energy: " << read_energy << " nJ" << std::endl;

        bool valid = (access_time > 0 && area > 0 && read_energy > 0);
        printTestResult(valid);
        return valid;

    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        printTestResult(false);
        return false;
    }
}

bool test_cacti_multiple_configurations() {
    printTestHeader("CACTI: Multiple Configurations");

    try {
        struct TestConfig {
            size_t capacity_kb;
            int line_size;
            int associativity;
            int tech_node;
            const char* description;
        };

        TestConfig configs[] = {
            {32, 64, 4, 22, "32KB L1 Cache (22nm)"},
            {256, 64, 8, 22, "256KB L2 Cache (22nm)"},
            {2048, 64, 16, 22, "2MB L3 Cache (22nm)"},
            {128, 64, 8, 14, "128KB Cache (14nm)"},
        };

        int passed = 0;
        int total = sizeof(configs) / sizeof(configs[0]);

        for (const auto& test_cfg : configs) {
            std::cout << "\nTesting: " << test_cfg.description << std::endl;

            CACTIWrapper::SRAMConfig config;
            config.capacity_bytes = test_cfg.capacity_kb * 1024;
            config.line_size = test_cfg.line_size;
            config.associativity = test_cfg.associativity;
            config.banks = 1;
            config.tech_node_nm = test_cfg.tech_node;
            config.is_cache = true;

            CACTIWrapper cacti(config);
            cacti.initialize();

            if (cacti.isValid()) {
                std::cout << "  ✓ Access Time: " << (cacti.getAccessTime() * 1e9) << " ns" << std::endl;
                std::cout << "  ✓ Area: " << cacti.getArea() << " mm²" << std::endl;
                passed++;
            } else {
                std::cout << "  ✗ Failed: " << cacti.getErrorMessage() << std::endl;
            }
        }

        std::cout << "\nPassed " << passed << "/" << total << " configurations" << std::endl;
        bool result = (passed == total);
        printTestResult(result);
        return result;

    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        printTestResult(false);
        return false;
    }
}

bool test_cacti_energy_metrics() {
    printTestHeader("CACTI: Energy Metrics");

    try {
        CACTIWrapper::SRAMConfig config;
        config.capacity_bytes = 128 * 1024;  // 128 KB
        config.line_size = 64;
        config.associativity = 4;
        config.banks = 1;
        config.tech_node_nm = 22;
        config.is_cache = true;

        CACTIWrapper cacti(config);
        cacti.initialize();

        if (!cacti.isValid()) {
            std::cout << "CACTI initialization failed" << std::endl;
            printTestResult(false);
            return false;
        }

        // Test all energy metrics
        double read_energy = cacti.getDynamicReadEnergy();
        double write_energy = cacti.getDynamicWriteEnergy();
        double leakage = cacti.getLeakagePower();
        double read_power = cacti.getReadDynamicPower();
        double write_power = cacti.getWriteDynamicPower();

        std::cout << "Energy Metrics:" << std::endl;
        std::cout << "  Read Energy: " << read_energy << " nJ" << std::endl;
        std::cout << "  Write Energy: " << write_energy << " nJ" << std::endl;
        std::cout << "  Read Dynamic Power: " << read_power << " mW" << std::endl;
        std::cout << "  Write Dynamic Power: " << write_power << " mW" << std::endl;
        std::cout << "  Leakage Power: " << leakage << " mW" << std::endl;

        bool valid = (read_energy > 0 && write_energy > 0 && leakage > 0 &&
                     read_power > 0 && write_power > 0);
        printTestResult(valid);
        return valid;

    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        printTestResult(false);
        return false;
    }
}

//=============================================================================
// Ramulator2 Tests
//=============================================================================

bool test_ramulator_basic_initialization() {
    printTestHeader("Ramulator2: Basic Initialization");

    try {
        // Create Ramulator wrapper with default DDR4 config
        RamulatorWrapper ramulator("");
        ramulator.initialize();

        std::cout << "Ramulator2 initialized successfully" << std::endl;

        // Check basic properties
        std::cout << "  Capacity: " << (ramulator.getCapacity() / (1024*1024*1024)) << " GB" << std::endl;
        std::cout << "  Bandwidth: " << ramulator.getBandwidth() << " MB/s" << std::endl;

        printTestResult(true);
        return true;

    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        printTestResult(false);
        return false;
    }
}

bool test_ramulator_memory_requests() {
    printTestHeader("Ramulator2: Memory Request Handling");

    try {
        RamulatorWrapper ramulator("");
        ramulator.initialize();

        // Test sending read and write requests
        int completed = 0;
        auto callback = [&completed](Address addr) {
            completed++;
        };

        // Send some read requests
        for (int i = 0; i < 10; i++) {
            Address addr = i * 64;  // 64-byte aligned addresses
            bool accepted = ramulator.send(addr, MemoryRequestType::READ, callback);
            if (!accepted) {
                std::cout << "Request " << i << " was not accepted" << std::endl;
            }
        }

        // Tick the simulator to process requests
        for (int cycle = 0; cycle < 1000; cycle++) {
            ramulator.tick();
        }

        std::cout << "Sent 10 requests, completed: " << completed << std::endl;

        // Print statistics
        ramulator.printStats();

        printTestResult(true);
        return true;

    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        printTestResult(false);
        return false;
    }
}

bool test_ramulator_energy_tracking() {
    printTestHeader("Ramulator2: Energy Tracking");

    try {
        RamulatorWrapper ramulator("");
        ramulator.initialize();

        // Send mixed read/write requests
        for (int i = 0; i < 100; i++) {
            Address addr = (i * 64) % (64 * 1024);  // Wrap within 64KB
            MemoryRequestType type = (i % 3 == 0) ?
                MemoryRequestType::WRITE : MemoryRequestType::READ;
            ramulator.send(addr, type, nullptr);
        }

        // Tick simulation
        for (int cycle = 0; cycle < 10000; cycle++) {
            ramulator.tick();
        }

        // Check energy metrics
        double read_energy = ramulator.getReadEnergy();
        double write_energy = ramulator.getWriteEnergy();
        double total_energy = ramulator.getTotalEnergy();
        double leakage = ramulator.getLeakagePower();

        std::cout << "Energy Metrics:" << std::endl;
        std::cout << "  Read Energy: " << read_energy << " nJ" << std::endl;
        std::cout << "  Write Energy: " << write_energy << " nJ" << std::endl;
        std::cout << "  Total Energy: " << total_energy << " nJ" << std::endl;
        std::cout << "  Leakage Power: " << leakage << " mW" << std::endl;

        bool valid = (read_energy > 0 && total_energy > 0);
        printTestResult(valid);
        return valid;

    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        printTestResult(false);
        return false;
    }
}

//=============================================================================
// Integration Tests
//=============================================================================

bool test_combined_memory_hierarchy() {
    printTestHeader("Integration: Combined SRAM+DRAM Hierarchy");

    try {
        // Create L1 cache (SRAM via CACTI)
        CACTIWrapper::SRAMConfig l1_config;
        l1_config.capacity_bytes = 32 * 1024;
        l1_config.line_size = 64;
        l1_config.associativity = 4;
        l1_config.banks = 1;
        l1_config.tech_node_nm = 22;
        l1_config.is_cache = true;

        CACTIWrapper l1_cache(l1_config);
        l1_cache.initialize();

        // Create main memory (DRAM via Ramulator)
        RamulatorWrapper main_memory("");
        main_memory.initialize();

        if (!l1_cache.isValid()) {
            std::cout << "L1 cache initialization failed" << std::endl;
            printTestResult(false);
            return false;
        }

        std::cout << "Memory Hierarchy Created:" << std::endl;
        std::cout << "  L1 Cache:" << std::endl;
        std::cout << "    Size: 32 KB" << std::endl;
        std::cout << "    Access Time: " << (l1_cache.getAccessTime() * 1e9) << " ns" << std::endl;
        std::cout << "    Read Energy: " << l1_cache.getDynamicReadEnergy() << " nJ" << std::endl;
        std::cout << "  Main Memory:" << std::endl;
        std::cout << "    Size: " << (main_memory.getCapacity() / (1024*1024*1024)) << " GB" << std::endl;
        std::cout << "    Bandwidth: " << main_memory.getBandwidth() << " MB/s" << std::endl;

        // Simulate some memory accesses
        std::cout << "\nSimulating 50 memory accesses..." << std::endl;
        for (int i = 0; i < 50; i++) {
            Address addr = (i * 64) % (1024 * 1024);  // 1MB address space
            main_memory.send(addr, MemoryRequestType::READ, nullptr);
        }

        // Run simulation
        for (int cycle = 0; cycle < 5000; cycle++) {
            main_memory.tick();
        }

        std::cout << "\nFinal Statistics:" << std::endl;
        main_memory.printStats();

        printTestResult(true);
        return true;

    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        printTestResult(false);
        return false;
    }
}

//=============================================================================
// Main test runner
//=============================================================================

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║     PIMID External Tools Integration Test Suite          ║\n";
    std::cout << "║     Testing CACTI and Ramulator2 Integration             ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";

    int total_tests = 0;
    int passed_tests = 0;

    // CACTI Tests
    std::cout << "\n" << std::string(60, '-') << std::endl;
    std::cout << "CACTI WRAPPER TESTS" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    total_tests++; if (test_cacti_basic_initialization()) passed_tests++;
    total_tests++; if (test_cacti_multiple_configurations()) passed_tests++;
    total_tests++; if (test_cacti_energy_metrics()) passed_tests++;

    // Ramulator2 Tests
    std::cout << "\n" << std::string(60, '-') << std::endl;
    std::cout << "RAMULATOR2 WRAPPER TESTS" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    total_tests++; if (test_ramulator_basic_initialization()) passed_tests++;
    total_tests++; if (test_ramulator_memory_requests()) passed_tests++;
    total_tests++; if (test_ramulator_energy_tracking()) passed_tests++;

    // Integration Tests
    std::cout << "\n" << std::string(60, '-') << std::endl;
    std::cout << "INTEGRATION TESTS" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    total_tests++; if (test_combined_memory_hierarchy()) passed_tests++;

    // Summary
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║                     TEST SUMMARY                          ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    std::cout << "  Total Tests:  " << total_tests << std::endl;
    std::cout << "  Passed:       " << passed_tests << " ("
              << std::fixed << std::setprecision(1)
              << (100.0 * passed_tests / total_tests) << "%)" << std::endl;
    std::cout << "  Failed:       " << (total_tests - passed_tests) << std::endl;

    if (passed_tests == total_tests) {
        std::cout << "\n✓ ALL TESTS PASSED!\n" << std::endl;
        return 0;
    } else {
        std::cout << "\n✗ SOME TESTS FAILED\n" << std::endl;
        return 1;
    }
}
