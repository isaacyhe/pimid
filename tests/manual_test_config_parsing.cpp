/**
 * @file manual_test_config_parsing.cpp
 * @brief Manual test to verify YAML config parsing works with actual config files
 */

#include "config/config_parser.h"
#include <iostream>
#include <string>

using namespace pimid::config;

int main(int argc, char** argv) {
    std::cout << "=== Testing YAML Configuration Parsing ===" << std::endl;
    std::cout << std::endl;

    // Test 1: Parse main memory configuration
    std::cout << "Test 1: Parsing memory_config.yaml" << std::endl;
    std::cout << "------------------------------------" << std::endl;

    ConfigParser parser;
    std::map<std::string, std::string> config;

    std::string config_file = "/home/user/pimid-dev/config/memory_config.yaml";

    if (!parser.parseFile(config_file, config)) {
        std::cerr << "ERROR: Failed to parse " << config_file << std::endl;
        return 1;
    }

    std::cout << "SUCCESS: Parsed " << config.size() << " configuration parameters" << std::endl;
    std::cout << std::endl;

    // Test 2: Verify DRAM parameters
    std::cout << "Test 2: Verifying DRAM Configuration" << std::endl;
    std::cout << "------------------------------------" << std::endl;

    std::vector<std::string> dram_params = {
        "dram.standard",
        "dram.speed_grade",
        "dram.organization.channels",
        "dram.organization.ranks_per_channel",
        "dram.organization.banks_per_chip",
        "dram.timing.tCL",
        "dram.timing.tRCD",
        "dram.timing.tRP",
        "dram.timing.tRAS"
    };

    bool all_found = true;
    for (const auto& param : dram_params) {
        if (config.find(param) != config.end()) {
            std::cout << "  ✓ " << param << " = " << config[param] << std::endl;
        } else {
            std::cout << "  ✗ " << param << " NOT FOUND" << std::endl;
            all_found = false;
        }
    }
    std::cout << std::endl;

    // Test 3: Verify SRAM parameters
    std::cout << "Test 3: Verifying SRAM Configuration" << std::endl;
    std::cout << "------------------------------------" << std::endl;

    std::vector<std::string> sram_params = {
        "sram.capacity_mb",
        "sram.organization.num_arrays",
        "sram.timing.access_time_ns",
        "sram.power.tech_node_nm",
        "sram.cacti.line_size_bytes",
        "sram.cacti.associativity"
    };

    for (const auto& param : sram_params) {
        if (config.find(param) != config.end()) {
            std::cout << "  ✓ " << param << " = " << config[param] << std::endl;
        } else {
            std::cout << "  ✗ " << param << " NOT FOUND" << std::endl;
            all_found = false;
        }
    }
    std::cout << std::endl;

    // Test 4: Verify STT-MRAM parameters
    std::cout << "Test 4: Verifying STT-MRAM Configuration" << std::endl;
    std::cout << "------------------------------------" << std::endl;

    std::vector<std::string> mram_params = {
        "stt_mram.capacity_mb",
        "stt_mram.organization.num_arrays",
        "stt_mram.timing.read_latency_ns",
        "stt_mram.timing.write_latency_ns",
        "stt_mram.reliability.write_endurance",
        "stt_mram.power.tech_node_nm"
    };

    for (const auto& param : mram_params) {
        if (config.find(param) != config.end()) {
            std::cout << "  ✓ " << param << " = " << config[param] << std::endl;
        } else {
            std::cout << "  ✗ " << param << " NOT FOUND" << std::endl;
            all_found = false;
        }
    }
    std::cout << std::endl;

    // Test 5: Test JSON export
    std::cout << "Test 5: Testing JSON Export" << std::endl;
    std::cout << "------------------------------------" << std::endl;

    std::string json = ConfigParser::toJSON(config);
    std::cout << "JSON export size: " << json.size() << " bytes" << std::endl;
    std::cout << "First 200 characters:" << std::endl;
    std::cout << json.substr(0, 200) << "..." << std::endl;
    std::cout << std::endl;

    // Summary
    std::cout << "=== Test Summary ===" << std::endl;
    if (all_found) {
        std::cout << "✓ ALL TESTS PASSED - YAML parsing working correctly!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ SOME TESTS FAILED - Check output above" << std::endl;
        return 1;
    }
}
