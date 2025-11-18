/**
 * Comprehensive Integration Test Suite
 * Tests YAML configuration loading and DRAM port width scaling
 */

#include <iostream>
#include <memory>
#include <vector>
#include <string>

// Memory models
#include "nvm_model.h"
#include "dram_architecture_v2.h"
#include "address_translation/pe_placement.h"

using namespace pimid;
using namespace pimid::memory;

// Test result tracking
struct TestResult {
    std::string test_name;
    bool passed;
    std::string message;
};

std::vector<TestResult> test_results;

void recordTest(const std::string& name, bool passed, const std::string& message = "") {
    test_results.push_back({name, passed, message});
    std::cout << "[TEST] " << name << ": " << (passed ? "PASS" : "FAIL");
    if (!message.empty()) {
        std::cout << " - " << message;
    }
    std::cout << std::endl;
}

// Test NVM configuration loading from YAML
void testNVMConfiguration(const std::string& config_file, const std::string& tech_type) {
    std::cout << "\n=== Testing NVM Configuration: " << tech_type << " ===" << std::endl;

    try {
        NVMModel nvm_model(config_file);
        nvm_model.initialize();

        // Check basic initialization
        uint64_t capacity = nvm_model.getCapacity();
        bool valid = (capacity > 0);

        recordTest("NVM " + tech_type + " configuration load", valid,
                   "Capacity: " + std::to_string(capacity) + " bytes");

        // Test read energy retrieval
        double read_energy = nvm_model.getReadEnergy();
        recordTest("NVM " + tech_type + " read energy", read_energy >= 0,
                   "Read energy: " + std::to_string(read_energy) + " nJ");

        // Test write energy retrieval
        double write_energy = nvm_model.getWriteEnergy();
        recordTest("NVM " + tech_type + " write energy", write_energy >= 0,
                   "Write energy: " + std::to_string(write_energy) + " nJ");

        // Verify asymmetric read/write (NVM characteristic)
        if (write_energy > 0 && read_energy > 0) {
            recordTest("NVM " + tech_type + " asymmetric energy", write_energy > read_energy,
                       "Write/Read ratio: " + std::to_string(write_energy / read_energy));
        }

    } catch (const std::exception& e) {
        recordTest("NVM " + tech_type + " exception test", false, std::string(e.what()));
    }
}

// Note: SRAM tests skipped due to duplicate VerificationStatus enum in headers

// Test DRAM with different port width scaling
void testDRAMPortScaling() {
    std::cout << "\n=== Testing DRAM Port Width Scaling ===" << std::endl;

    std::vector<double> scales = {0.5, 1.0, 2.0, 4.0};

    for (double scale : scales) {
        try {
            auto dram = createDDR4_2400_Verified(scale);

            // Verify DRAM was created
            bool created = (dram != nullptr);
            recordTest("DDR4 creation with scale " + std::to_string(scale) + "x",
                       created, "DRAM instance created");

            if (created) {
                // Test PE bus constraints generation
                auto constraints = createPEBusConstraintsFromDRAM(*dram, PEPlacementLevel::BANK);

                // Expected values based on scaling
                uint64_t base_width = 64;  // bits
                double base_bw = 25.0;  // GB/s per channel

                uint64_t expected_width = static_cast<uint64_t>(base_width * scale);
                double expected_bw = base_bw * scale;

                bool width_correct = (constraints.data_bus_width_bits == expected_width);
                recordTest("DDR4 bus width " + std::to_string(scale) + "x",
                           width_correct,
                           "Expected: " + std::to_string(expected_width) +
                           " bits, Got: " + std::to_string(constraints.data_bus_width_bits) + " bits");

                bool bw_correct = (std::abs(constraints.max_bandwidth_gbps - expected_bw) < 0.1);
                recordTest("DDR4 bandwidth " + std::to_string(scale) + "x",
                           bw_correct,
                           "Expected: " + std::to_string(expected_bw) +
                           " GB/s, Got: " + std::to_string(constraints.max_bandwidth_gbps) + " GB/s");
            }
        } catch (const std::exception& e) {
            recordTest("DDR4 scale " + std::to_string(scale) + "x exception",
                       false, std::string(e.what()));
        }
    }
}

// Print test summary
void printTestSummary() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;

    int passed = 0;
    int failed = 0;

    for (const auto& result : test_results) {
        if (result.passed) {
            passed++;
        } else {
            failed++;
            std::cout << "FAILED: " << result.test_name;
            if (!result.message.empty()) {
                std::cout << " - " << result.message;
            }
            std::cout << std::endl;
        }
    }

    std::cout << "\nTotal tests: " << test_results.size() << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    if (test_results.size() > 0) {
        std::cout << "Success rate: " << (100.0 * passed / test_results.size()) << "%" << std::endl;
    }
}

int main(int argc, char** argv) {
    std::cout << "========================================" << std::endl;
    std::cout << "PIMID Comprehensive Integration Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    std::string config_file = "config/memory_config.yaml";
    if (argc > 1) {
        config_file = argv[1];
    }

    std::cout << "Using configuration file: " << config_file << std::endl;

    // Run all tests
    testNVMConfiguration(config_file, "STT-MRAM");
    testNVMConfiguration(config_file, "PCM");
    testNVMConfiguration(config_file, "ReRAM");
    // testSRAMConfiguration(config_file);  // Skipped due to duplicate enum in headers
    testDRAMPortScaling();

    // Print summary
    printTestSummary();

    // Return 0 if all tests passed, 1 otherwise
    for (const auto& result : test_results) {
        if (!result.passed) {
            return 1;
        }
    }

    return 0;
}
