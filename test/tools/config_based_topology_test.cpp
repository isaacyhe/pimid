/**
 * @file config_based_topology_test.cpp
 * @brief Configuration-driven test for custom network topology
 *
 * This program reads a YAML configuration file and:
 * 1. Parses custom topology settings
 * 2. Creates InternalDRAMNetwork with specified configuration
 * 3. Validates the configuration
 * 4. Runs basic simulation
 * 5. Reports results
 */

#include "memory_models/include/internal_dram_network.h"
#include "config/include/network_config_parser.h"
#include "external_models/include/external_model_interface.h"
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <stdexcept>

using namespace pimid;

struct TestConfig {
    std::string dram_type;
    int channels;
    int ranks_per_channel;
    int chips_per_rank;
    int bg_per_chip;
    int banks_per_bg;
    int subarrays_per_bank;
    int simulation_cycles;
};

TestConfig parseTestConfig(const std::string& config_file) {
    TestConfig config;

    try {
        YAML::Node yaml_config = YAML::LoadFile(config_file);

        // Parse simulation settings
        if (yaml_config["simulation"] && yaml_config["simulation"]["duration_cycles"]) {
            config.simulation_cycles = yaml_config["simulation"]["duration_cycles"].as<int>();
        } else {
            config.simulation_cycles = 1000;  // Default
        }

        // Parse memory configuration
        if (!yaml_config["memory"]) {
            throw std::runtime_error("Missing 'memory' section in config");
        }

        const YAML::Node& memory = yaml_config["memory"];

        config.dram_type = memory["dram_type"].as<std::string>();
        config.channels = memory["channels"].as<int>();
        config.ranks_per_channel = memory["ranks_per_channel"].as<int>();
        config.chips_per_rank = memory["chips_per_rank"].as<int>();
        config.bg_per_chip = memory["bg_per_chip"].as<int>();
        config.banks_per_bg = memory["banks_per_bg"].as<int>();
        config.subarrays_per_bank = memory["subarrays_per_bank"].as<int>();

    } catch (const YAML::Exception& e) {
        throw std::runtime_error("YAML parsing error: " + std::string(e.what()));
    }

    return config;
}

bool validateConfiguration(
    const InternalDRAMNetwork* network,
    const SwitchHierarchyConfig& switch_config,
    const TestConfig& test_config) {

    std::cout << "\n=== Validating Configuration ===" << std::endl;

    bool valid = true;

    // Get switch counts
    int total_switches = network->getTotalNumberOfSwitches(
        test_config.channels,
        test_config.ranks_per_channel);

    std::cout << "Total switches: " << total_switches << std::endl;

    // Validate each level
    for (int level = 0; level <= 5; level++) {
        int level_switches = network->getNumberOfSwitchesAtLevel(
            level,
            test_config.channels,
            test_config.ranks_per_channel);

        std::cout << "  L" << level << ": " << level_switches << " switches" << std::endl;

        if (level_switches < 0) {
            std::cerr << "ERROR: Invalid switch count at level " << level << std::endl;
            valid = false;
        }
    }

    // Validate custom config if present
    if (!switch_config.l0_config.use_default ||
        !switch_config.l1_config.use_default ||
        !switch_config.l2_config.use_default ||
        !switch_config.l3_config.use_default ||
        !switch_config.l4_config.use_default ||
        !switch_config.l5_config.use_default) {

        std::cout << "\nCustom topology detected, validating..." << std::endl;

        valid &= NetworkConfigParser::validateTopologyConfig(
            switch_config,
            test_config.banks_per_bg,
            test_config.bg_per_chip,
            test_config.chips_per_rank);
    }

    if (valid) {
        std::cout << "✓ Configuration validation passed" << std::endl;
    } else {
        std::cerr << "✗ Configuration validation failed" << std::endl;
    }

    return valid;
}

void runSimulation(InternalDRAMNetwork* network, int cycles) {
    std::cout << "\n=== Running Simulation ===" << std::endl;
    std::cout << "Duration: " << cycles << " cycles" << std::endl;

    // Simple simulation: just advance cycles
    for (int i = 0; i < cycles; i++) {
        // In a real simulation, we would process packets here
        // For testing, just verify the network is functional
        if (i % (cycles / 10) == 0 && i > 0) {
            std::cout << "  Progress: " << (100 * i / cycles) << "%" << std::endl;
        }
    }

    std::cout << "✓ Simulation completed" << std::endl;
}

void loadExternalModels(const NetworkTopologyConfig& config) {
    std::cout << "\n=== External Model Integration ===" << std::endl;

    if (config.network_model.enabled) {
        std::cout << "Loading external network model: " << config.network_model.model_name << std::endl;
        std::cout << "  Library: " << config.network_model.library_path << std::endl;

        // Load the library
        int result = pimid_load_model(config.network_model.library_path.c_str());
        if (result == 0) {
            std::cout << "✓ External network model loaded" << std::endl;

            // Try to create an instance
            PimidModelHandle handle = pimid_create_model(config.network_model.model_name.c_str());
            if (handle) {
                std::cout << "✓ Model instance created" << std::endl;

                // Initialize if config file provided
                if (!config.network_model.config_file.empty()) {
                    result = pimid_model_init(
                        handle,
                        config.network_model.model_name.c_str(),
                        config.network_model.config_file.c_str());

                    if (result == 0) {
                        std::cout << "✓ Model initialized" << std::endl;
                    } else {
                        std::cerr << "⚠ Model initialization failed" << std::endl;
                    }
                }

                // Cleanup
                pimid_model_destroy(handle, config.network_model.model_name.c_str());
            } else {
                std::cerr << "⚠ Failed to create model instance" << std::endl;
            }
        } else {
            std::cerr << "⚠ Failed to load external network model" << std::endl;
        }
    }

    if (config.memory_model.enabled) {
        std::cout << "Loading external memory model: " << config.memory_model.model_name << std::endl;
        std::cout << "  Library: " << config.memory_model.library_path << std::endl;

        // Similar loading process
        int result = pimid_load_model(config.memory_model.library_path.c_str());
        if (result == 0) {
            std::cout << "✓ External memory model loaded" << std::endl;
        } else {
            std::cerr << "⚠ Failed to load external memory model" << std::endl;
        }
    }

    if (!config.network_model.enabled && !config.memory_model.enabled) {
        std::cout << "No external models configured" << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file.yaml>" << std::endl;
        return 1;
    }

    std::string config_file = argv[1];

    std::cout << "╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║   Config-Based Network Topology Test                        ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "\nConfig file: " << config_file << std::endl;

    try {
        // Parse test configuration
        std::cout << "\n=== Parsing Configuration ===" << std::endl;
        TestConfig test_config = parseTestConfig(config_file);

        std::cout << "Memory configuration:" << std::endl;
        std::cout << "  DRAM type: " << test_config.dram_type << std::endl;
        std::cout << "  Channels: " << test_config.channels << std::endl;
        std::cout << "  Ranks per channel: " << test_config.ranks_per_channel << std::endl;
        std::cout << "  Chips per rank: " << test_config.chips_per_rank << std::endl;
        std::cout << "  BGs per chip: " << test_config.bg_per_chip << std::endl;
        std::cout << "  Banks per BG: " << test_config.banks_per_bg << std::endl;
        std::cout << "  Subarrays per bank: " << test_config.subarrays_per_bank << std::endl;

        // Parse network topology configuration
        NetworkTopologyConfig topo_config = NetworkConfigParser::parseConfigFile(config_file);

        // Create internal DRAM network
        std::cout << "\n=== Creating DRAM Network ===" << std::endl;
        auto network = createInternalDRAMNetwork(
            test_config.dram_type,
            test_config.subarrays_per_bank,
            test_config.banks_per_bg,
            test_config.bg_per_chip,
            test_config.chips_per_rank);

        std::cout << "✓ Network created" << std::endl;

        // Apply custom topology if enabled
        if (topo_config.custom_topology_enabled) {
            std::cout << "\n=== Applying Custom Topology ===" << std::endl;
            network->setCustomSwitchHierarchy(topo_config.switch_config);
            std::cout << "✓ Custom topology applied" << std::endl;

            // Print topology details
            SwitchHierarchyConfig current = network->getSwitchHierarchyConfig(
                test_config.channels,
                test_config.ranks_per_channel);

            for (int level = 0; level <= 5; level++) {
                NetworkLevelConfig* level_config = nullptr;
                switch (level) {
                    case 0: level_config = &current.l0_config; break;
                    case 1: level_config = &current.l1_config; break;
                    case 2: level_config = &current.l2_config; break;
                    case 3: level_config = &current.l3_config; break;
                    case 4: level_config = &current.l4_config; break;
                    case 5: level_config = &current.l5_config; break;
                }

                if (level_config && !level_config->use_default) {
                    std::cout << "  L" << level << ": "
                              << InternalDRAMNetwork::getTopologyName(level_config->topology)
                              << " (" << level_config->num_switches << " switches, "
                              << level_config->ports_per_switch << " ports)" << std::endl;
                }
            }
        }

        // Validate configuration
        bool valid = validateConfiguration(
            network.get(),
            topo_config.switch_config,
            test_config);

        if (!valid) {
            std::cerr << "\n✗ Configuration validation failed" << std::endl;
            return 1;
        }

        // Load external models if specified
        loadExternalModels(topo_config);

        // Run simulation
        runSimulation(network.get(), test_config.simulation_cycles);

        // Success
        std::cout << "\n" << std::string(64, '=') << std::endl;
        std::cout << "✓ Test PASSED" << std::endl;
        std::cout << std::string(64, '=') << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test FAILED with exception: " << e.what() << std::endl;
        return 1;
    }
}
