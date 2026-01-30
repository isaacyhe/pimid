/**
 * @file test_dram_config_validation.cpp
 * @brief Config-based validation test for GARNET H-tree with all DRAM types
 *
 * This test validates that all 12 DRAM types can be configured with proper
 * VN/VC/router pipeline parameters via YAML config files.
 */

#include "internal_dram_network.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <sstream>

using namespace pimid;

// Helper function to find config file in multiple possible locations
std::string findConfigFile(const std::string& relative_path) {
    // List of possible base directories to search
    std::vector<std::string> search_paths = {
        "",                           // Current directory
        "../",                        // One level up
        "../../",                     // Two levels up (from build/test/)
        "../../../",                  // Three levels up
        "/home/user/pimid-dev/",     // Absolute path
    };

    for (const auto& base : search_paths) {
        std::string full_path = base + relative_path;
        std::ifstream test(full_path);
        if (test.good()) {
            return full_path;
        }
    }

    // Return original path if not found (will fail gracefully)
    return relative_path;
}

// Simple YAML parser for validation test
class SimpleYAMLParser {
public:
    std::map<std::string, std::string> parse(const std::string& filename) {
        std::map<std::string, std::string> values;
        std::ifstream file(filename);

        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << filename << std::endl;
            return values;
        }

        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') continue;

            // Simple key: value parsing
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;

            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // Trim whitespace and quotes
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t\""));
            value.erase(value.find_last_not_of(" \t\"") + 1);

            values[key] = value;
        }

        return values;
    }
};

struct DRAMConfigTest {
    std::string name;
    std::string config_file;
    int expected_num_subarrays;
    int expected_num_banks;
    int expected_num_bgs;
    int expected_num_chips;

    // Expected network parameters
    int expected_vn;
    int expected_vc_per_vn;
    std::string expected_router_pipeline;
    int expected_router_latency;
    int expected_input_buffer_depth;
    int expected_output_buffer_depth;
};

bool validateConfig(const DRAMConfigTest& test) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Testing: " << test.name << std::endl;
    std::cout << "Config: " << test.config_file << std::endl;
    std::cout << "========================================" << std::endl;

    // Parse config file
    SimpleYAMLParser parser;
    auto config_values = parser.parse(test.config_file);

    if (config_values.empty()) {
        std::cerr << "❌ Failed to parse config file" << std::endl;
        return false;
    }

    // Validate DRAM type
    if (config_values["type"] != test.name) {
        std::cerr << "❌ DRAM type mismatch: expected " << test.name
                  << ", got " << config_values["type"] << std::endl;
        return false;
    }
    std::cout << "✅ DRAM type: " << config_values["type"] << std::endl;

    // Validate organization
    if (std::stoi(config_values["num_subarrays"]) != test.expected_num_subarrays) {
        std::cerr << "❌ Subarray count mismatch" << std::endl;
        return false;
    }
    std::cout << "✅ Subarrays: " << config_values["num_subarrays"] << std::endl;

    // Validate network topology
    if (config_values["topology"] != "H_TREE") {
        std::cerr << "❌ Topology should be H_TREE, got " << config_values["topology"] << std::endl;
        return false;
    }
    std::cout << "✅ Topology: " << config_values["topology"] << std::endl;

    // Validate Virtual Networks (VN)
    if (std::stoi(config_values["virtual_networks"]) != test.expected_vn) {
        std::cerr << "❌ Virtual Networks mismatch: expected " << test.expected_vn
                  << ", got " << config_values["virtual_networks"] << std::endl;
        return false;
    }
    std::cout << "✅ Virtual Networks (VN): " << config_values["virtual_networks"] << std::endl;

    // Validate Virtual Channels per VN
    if (std::stoi(config_values["virtual_channels_per_vn"]) != test.expected_vc_per_vn) {
        std::cerr << "❌ VCs per VN mismatch: expected " << test.expected_vc_per_vn
                  << ", got " << config_values["virtual_channels_per_vn"] << std::endl;
        return false;
    }
    std::cout << "✅ Virtual Channels per VN: " << config_values["virtual_channels_per_vn"] << std::endl;

    // Validate router pipeline complexity
    if (config_values["router_pipeline"] != test.expected_router_pipeline) {
        std::cerr << "❌ Router pipeline mismatch: expected " << test.expected_router_pipeline
                  << ", got " << config_values["router_pipeline"] << std::endl;
        return false;
    }
    std::cout << "✅ Router Pipeline: " << config_values["router_pipeline"] << std::endl;

    // Validate router latency
    if (std::stoi(config_values["router_latency"]) != test.expected_router_latency) {
        std::cerr << "❌ Router latency mismatch: expected " << test.expected_router_latency
                  << ", got " << config_values["router_latency"] << std::endl;
        return false;
    }
    std::cout << "✅ Router Latency: " << config_values["router_latency"] << " cycles" << std::endl;

    // Validate buffer depths
    if (std::stoi(config_values["input_buffer_depth"]) != test.expected_input_buffer_depth) {
        std::cerr << "❌ Input buffer depth mismatch" << std::endl;
        return false;
    }
    std::cout << "✅ Input Buffer Depth: " << config_values["input_buffer_depth"] << std::endl;

    if (std::stoi(config_values["output_buffer_depth"]) != test.expected_output_buffer_depth) {
        std::cerr << "❌ Output buffer depth mismatch" << std::endl;
        return false;
    }
    std::cout << "✅ Output Buffer Depth: " << config_values["output_buffer_depth"] << std::endl;

    // Validate router bypass
    if (config_values["enable_router_bypass"] != "true") {
        std::cerr << "❌ Router bypass should be enabled for DRAM" << std::endl;
        return false;
    }
    std::cout << "✅ Router Bypass: enabled" << std::endl;

    // Test network creation
    try {
        auto dram_network = createInternalDRAMNetwork(
            test.name,
            test.expected_num_subarrays,
            test.expected_num_banks,
            test.expected_num_bgs,
            test.expected_num_chips
        );

        dram_network->enableGarnetSimulation(true);
        std::cout << "✅ Network created and GARNET enabled" << std::endl;

        // Test packet transfer
        InternalNetworkPacket packet;
        packet.packet_id = 0;
        packet.source_bank = 0;
        packet.dest_bank = 1;
        packet.source_subarray = 0;
        packet.dest_subarray = 0;
        packet.data_bytes = 64;
        packet.completed = false;

        if (dram_network->sendPacket(packet)) {
            std::cout << "✅ Test packet sent successfully" << std::endl;
        } else {
            std::cout << "⚠️  Packet rejected (network busy)" << std::endl;
        }

        // Simulate a few cycles
        for (int i = 0; i < 100; i++) {
            dram_network->tick();
        }

        std::cout << "\n✅ " << test.name << " VALIDATION PASSED" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "❌ Network creation failed: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << " GARNET H-Tree Config Validation Test" << std::endl;
    std::cout << " Testing VN/VC/Router Pipeline Parameters" << std::endl;
    std::cout << " for All 12 DRAM Types" << std::endl;
    std::cout << "============================================" << std::endl;

    // Define test cases for all 12 DRAM types
    // All should have: VN=2, VC_per_VN=1, router_pipeline=MINIMAL
    std::vector<DRAMConfigTest> test_cases = {
        // DDR Family
        {"DDR3", findConfigFile("pimid/configs/dram/ddr3_htree.yaml"), 16, 4, 2, 8, 2, 1, "MINIMAL", 1, 2, 2},
        {"DDR4", findConfigFile("pimid/configs/dram/ddr4_htree.yaml"), 16, 4, 4, 8, 2, 1, "MINIMAL", 1, 2, 2},
        {"DDR4-RVRR", findConfigFile("pimid/configs/dram/ddr4_rvrr_htree.yaml"), 16, 4, 4, 8, 2, 1, "MINIMAL", 1, 2, 2},
        {"DDR4-VRR", findConfigFile("pimid/configs/dram/ddr4_vrr_htree.yaml"), 16, 4, 4, 8, 2, 1, "MINIMAL", 1, 2, 2},
        {"DDR5", findConfigFile("pimid/configs/dram/ddr5_htree.yaml"), 32, 4, 8, 8, 2, 1, "MINIMAL", 1, 2, 2},
        {"DDR5-RVRR", findConfigFile("pimid/configs/dram/ddr5_rvrr_htree.yaml"), 32, 4, 8, 8, 2, 1, "MINIMAL", 1, 2, 2},
        {"DDR5-VRR", findConfigFile("pimid/configs/dram/ddr5_vrr_htree.yaml"), 32, 4, 8, 8, 2, 1, "MINIMAL", 1, 2, 2},

        // Mobile DRAM
        {"LPDDR5", findConfigFile("pimid/configs/dram/lpddr5_htree.yaml"), 16, 4, 4, 8, 2, 1, "MINIMAL", 1, 2, 2},

        // Graphics DRAM
        {"GDDR6", findConfigFile("pimid/configs/dram/gddr6_htree.yaml"), 16, 4, 4, 2, 2, 1, "MINIMAL", 1, 2, 2},

        // High-Bandwidth Memory
        {"HBM", findConfigFile("pimid/configs/dram/hbm_htree.yaml"), 16, 2, 4, 8, 2, 1, "MINIMAL", 1, 2, 2},
        {"HBM2", findConfigFile("pimid/configs/dram/hbm2_htree.yaml"), 16, 4, 4, 8, 2, 1, "MINIMAL", 1, 2, 2},
        {"HBM3", findConfigFile("pimid/configs/dram/hbm3_htree.yaml"), 32, 4, 4, 8, 2, 1, "MINIMAL", 1, 2, 2},
    };

    int passed = 0;
    int failed = 0;

    for (const auto& test : test_cases) {
        if (validateConfig(test)) {
            passed++;
        } else {
            failed++;
            std::cerr << "❌ " << test.name << " FAILED" << std::endl;
        }
    }

    std::cout << "\n============================================" << std::endl;
    std::cout << " Test Summary" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "Total tests: " << test_cases.size() << std::endl;
    std::cout << "Passed:      " << passed << std::endl;
    std::cout << "Failed:      " << failed << std::endl;
    std::cout << "============================================" << std::endl;

    if (failed == 0) {
        std::cout << "\n✅ ALL VALIDATION TESTS PASSED!" << std::endl;
        std::cout << "All 12 DRAM types successfully configured with:" << std::endl;
        std::cout << "  - Virtual Networks (VN): 2" << std::endl;
        std::cout << "  - Virtual Channels per VN: 1" << std::endl;
        std::cout << "  - Router Pipeline: MINIMAL" << std::endl;
        std::cout << "  - Router Latency: 1 cycle" << std::endl;
        std::cout << "  - Buffer Depth: 2 (input/output)" << std::endl;
        std::cout << "  - Router Bypass: Enabled" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ SOME TESTS FAILED" << std::endl;
        return 1;
    }
}
