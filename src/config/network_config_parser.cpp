/**
 * @file network_config_parser.cpp
 * @brief Implementation of network topology and external model config parser
 */

#include "config/network_config_parser.h"
#include <iostream>
#include <stdexcept>
#include <fstream>

namespace pimid {

TopologyType NetworkConfigParser::parseTopologyType(const std::string& topology_str) {
    if (topology_str == "BUS") return TopologyType::BUS;
    if (topology_str == "CROSSBAR") return TopologyType::CROSSBAR;
    if (topology_str == "MESH_2D") return TopologyType::MESH_2D;
    if (topology_str == "TORUS_2D") return TopologyType::TORUS_2D;
    if (topology_str == "FAT_TREE") return TopologyType::FAT_TREE;
    if (topology_str == "H_TREE") return TopologyType::H_TREE;
    if (topology_str == "CUSTOM") return TopologyType::CUSTOM;

    throw std::runtime_error("Unknown topology type: " + topology_str);
}

NetworkLevelConfig NetworkConfigParser::parseNetworkLevel(
    const YAML::Node& level_node,
    int level_num) {

    NetworkLevelConfig config;
    config.level = level_num;

    if (!level_node) {
        config.use_default = true;
        return config;
    }

    // Check if using default configuration
    if (level_node["use_default"]) {
        config.use_default = level_node["use_default"].as<bool>();
    } else {
        config.use_default = false;
    }

    if (config.use_default) {
        return config;
    }

    // Parse custom configuration
    if (!level_node["topology"]) {
        throw std::runtime_error("Topology type required when use_default is false");
    }

    std::string topology_str = level_node["topology"].as<std::string>();
    config.topology = parseTopologyType(topology_str);

    if (level_node["num_switches"]) {
        config.num_switches = level_node["num_switches"].as<int>();
    } else {
        throw std::runtime_error("num_switches required when use_default is false");
    }

    if (level_node["ports_per_switch"]) {
        config.ports_per_switch = level_node["ports_per_switch"].as<int>();
    } else {
        throw std::runtime_error("ports_per_switch required when use_default is false");
    }

    if (level_node["num_endpoints"]) {
        config.num_endpoints = level_node["num_endpoints"].as<int>();
    } else {
        throw std::runtime_error("num_endpoints required when use_default is false");
    }

    return config;
}

SwitchHierarchyConfig NetworkConfigParser::parseCustomTopology(
    const YAML::Node& config_node,
    int num_channels,
    int ranks_per_channel) {

    SwitchHierarchyConfig config;
    config.num_channels = num_channels;
    config.ranks_per_channel = ranks_per_channel;

    if (!config_node) {
        // No custom topology, use defaults
        config.l0_config.use_default = true;
        config.l1_config.use_default = true;
        config.l2_config.use_default = true;
        config.l3_config.use_default = true;
        config.l4_config.use_default = true;
        config.l5_config.use_default = true;
        return config;
    }

    // Parse each level
    config.l0_config = parseNetworkLevel(config_node["l0"], 0);
    config.l1_config = parseNetworkLevel(config_node["l1"], 1);
    config.l2_config = parseNetworkLevel(config_node["l2"], 2);
    config.l3_config = parseNetworkLevel(config_node["l3"], 3);
    config.l4_config = parseNetworkLevel(config_node["l4"], 4);
    config.l5_config = parseNetworkLevel(config_node["l5"], 5);

    return config;
}

ExternalModelConfig NetworkConfigParser::parseExternalModel(
    const YAML::Node& model_node) {

    ExternalModelConfig config;

    if (!model_node) {
        config.enabled = false;
        return config;
    }

    config.enabled = model_node["enabled"] ? model_node["enabled"].as<bool>() : false;

    if (config.enabled) {
        if (!model_node["library_path"]) {
            throw std::runtime_error("library_path required when external model is enabled");
        }
        config.library_path = model_node["library_path"].as<std::string>();

        if (!model_node["model_name"]) {
            throw std::runtime_error("model_name required when external model is enabled");
        }
        config.model_name = model_node["model_name"].as<std::string>();

        if (model_node["config_file"]) {
            config.config_file = model_node["config_file"].as<std::string>();
        }
    }

    return config;
}

NetworkTopologyConfig NetworkConfigParser::parseConfigFile(const std::string& config_file) {
    NetworkTopologyConfig result;

    try {
        YAML::Node config = YAML::LoadFile(config_file);

        // Get memory configuration
        int num_channels = 1;
        int ranks_per_channel = 1;

        if (config["memory"]) {
            const YAML::Node& memory = config["memory"];

            if (memory["channels"]) {
                num_channels = memory["channels"].as<int>();
            }

            if (memory["ranks_per_channel"]) {
                ranks_per_channel = memory["ranks_per_channel"].as<int>();
            }

            // Parse custom topology if present
            if (memory["network"] && memory["network"]["custom_topology"]) {
                const YAML::Node& custom_topo = memory["network"]["custom_topology"];

                result.custom_topology_enabled =
                    custom_topo["enabled"] ? custom_topo["enabled"].as<bool>() : false;

                if (result.custom_topology_enabled) {
                    result.switch_config = parseCustomTopology(
                        custom_topo,
                        num_channels,
                        ranks_per_channel);
                }
            }
        }

        // Parse external models if present
        if (config["external_models"]) {
            const YAML::Node& ext_models = config["external_models"];

            if (ext_models["network"]) {
                result.network_model = parseExternalModel(ext_models["network"]);
            }

            if (ext_models["memory"]) {
                result.memory_model = parseExternalModel(ext_models["memory"]);
            }
        }

        std::cout << "[NetworkConfigParser] Configuration parsed successfully" << std::endl;
        std::cout << "  Custom topology: " << (result.custom_topology_enabled ? "enabled" : "disabled") << std::endl;
        std::cout << "  External network model: " << (result.network_model.enabled ? "enabled" : "disabled") << std::endl;
        std::cout << "  External memory model: " << (result.memory_model.enabled ? "enabled" : "disabled") << std::endl;

    } catch (const YAML::Exception& e) {
        throw std::runtime_error("YAML parsing error: " + std::string(e.what()));
    }

    return result;
}

bool NetworkConfigParser::validateTopologyConfig(
    const SwitchHierarchyConfig& config,
    int num_banks_per_bg,
    int num_bg_per_chip,
    int num_chips_per_rank) {

    // Basic validation rules
    bool valid = true;

    // Validate L0 (bank level within BG)
    if (!config.l0_config.use_default) {
        if (config.l0_config.num_endpoints != num_banks_per_bg) {
            std::cerr << "[Validation] Warning: L0 num_endpoints (" << config.l0_config.num_endpoints
                      << ") doesn't match banks_per_bg (" << num_banks_per_bg << ")" << std::endl;
            valid = false;
        }
    }

    // Validate L1 (BG level within chip)
    if (!config.l1_config.use_default) {
        int total_bgs = num_bg_per_chip * config.num_channels * config.ranks_per_channel;
        // L1 connects all BGs, so endpoints could be num_bg_per_chip or total_bgs
        // This is flexible based on design
    }

    // Validate L2 (chip level within rank)
    if (!config.l2_config.use_default) {
        if (config.l2_config.num_endpoints != num_chips_per_rank) {
            std::cerr << "[Validation] Warning: L2 num_endpoints (" << config.l2_config.num_endpoints
                      << ") doesn't match chips_per_rank (" << num_chips_per_rank << ")" << std::endl;
            // Not a hard error, might be intentional
        }
    }

    // Check for positive values
    auto checkLevel = [&](const NetworkLevelConfig& level, const std::string& name) {
        if (!level.use_default) {
            if (level.num_switches <= 0 || level.ports_per_switch <= 0 || level.num_endpoints <= 0) {
                std::cerr << "[Validation] Error: " << name << " has invalid values" << std::endl;
                return false;
            }
        }
        return true;
    };

    valid &= checkLevel(config.l0_config, "L0");
    valid &= checkLevel(config.l1_config, "L1");
    valid &= checkLevel(config.l2_config, "L2");
    valid &= checkLevel(config.l3_config, "L3");
    valid &= checkLevel(config.l4_config, "L4");
    valid &= checkLevel(config.l5_config, "L5");

    return valid;
}

NetworkConfig NetworkConfigParser::parseNoCConfig(const std::string& config_file) {
    try {
        YAML::Node config = YAML::LoadFile(config_file);

        if (config["noc"]) {
            return parseNoCNode(config["noc"]);
        }

        // Return defaults if no noc section
        std::cout << "[NetworkConfigParser] No 'noc' section found, using defaults" << std::endl;
        return NetworkConfig();

    } catch (const YAML::Exception& e) {
        throw std::runtime_error("YAML parsing error in NoC config: " + std::string(e.what()));
    }
}

NetworkConfig NetworkConfigParser::parseNoCNode(const YAML::Node& noc_node) {
    NetworkConfig config;

    if (!noc_node) {
        return config;  // Return defaults
    }

    // Parse topology
    if (noc_node["topology"]) {
        std::string topo = noc_node["topology"].as<std::string>();
        if (topo == "MESH_2D") config.topology = NetworkTopology::MESH_2D;
        else if (topo == "MESH_3D") config.topology = NetworkTopology::MESH_3D;
        else if (topo == "TORUS_2D") config.topology = NetworkTopology::TORUS_2D;
        else if (topo == "TORUS_3D") config.topology = NetworkTopology::TORUS_3D;
        else if (topo == "DRAGONFLY") config.topology = NetworkTopology::DRAGONFLY;
        else if (topo == "FAT_TREE") config.topology = NetworkTopology::FAT_TREE;
        else if (topo == "H_TREE") config.topology = NetworkTopology::H_TREE;
        else if (topo == "CROSSBAR") config.topology = NetworkTopology::CROSSBAR;
        else {
            std::cerr << "[NetworkConfigParser] Unknown topology: " << topo << ", using MESH_2D" << std::endl;
        }
    }

    // Parse routing algorithm
    if (noc_node["routing"]) {
        std::string routing = noc_node["routing"].as<std::string>();
        if (routing == "XY") config.routing = RoutingAlgorithm::XY;
        else if (routing == "XYZ") config.routing = RoutingAlgorithm::XYZ;
        else if (routing == "ADAPTIVE") config.routing = RoutingAlgorithm::ADAPTIVE;
        else if (routing == "WEST_FIRST") config.routing = RoutingAlgorithm::WEST_FIRST;
        else if (routing == "NORTH_LAST") config.routing = RoutingAlgorithm::NORTH_LAST;
        else if (routing == "MINIMAL") config.routing = RoutingAlgorithm::MINIMAL;
        else if (routing == "VALIANT") config.routing = RoutingAlgorithm::VALIANT;
    }

    // Parse dimensions
    if (noc_node["num_rows"]) {
        config.num_rows = noc_node["num_rows"].as<uint32_t>();
    }
    if (noc_node["num_cols"]) {
        config.num_cols = noc_node["num_cols"].as<uint32_t>();
    }
    if (noc_node["num_layers"]) {
        config.num_layers = noc_node["num_layers"].as<uint32_t>();
    }

    // Parse virtual channels
    if (noc_node["virtual_channels_per_vn"]) {
        config.virtual_channels_per_vn = noc_node["virtual_channels_per_vn"].as<uint32_t>();
    }
    if (noc_node["virtual_networks"]) {
        config.virtual_networks = noc_node["virtual_networks"].as<uint32_t>();
    }

    // Parse link parameters
    if (noc_node["link_width_bytes"]) {
        config.link_width_bytes = noc_node["link_width_bytes"].as<uint32_t>();
    }
    if (noc_node["link_latency"]) {
        config.link_latency = noc_node["link_latency"].as<uint32_t>();
    }

    // Parse router parameters
    if (noc_node["router_latency"]) {
        config.router_latency = noc_node["router_latency"].as<uint32_t>();
    }
    if (noc_node["enable_router_bypass"]) {
        config.enable_router_bypass = noc_node["enable_router_bypass"].as<bool>();
    }

    // Parse buffer parameters
    if (noc_node["input_buffer_depth"]) {
        config.input_buffer_depth = noc_node["input_buffer_depth"].as<uint32_t>();
    }
    if (noc_node["output_buffer_depth"]) {
        config.output_buffer_depth = noc_node["output_buffer_depth"].as<uint32_t>();
    }

    // Parse Garnet config file path
    if (noc_node["garnet_config_path"]) {
        config.garnet_config_path = noc_node["garnet_config_path"].as<std::string>();
    }

    std::cout << "[NetworkConfigParser] NoC configuration parsed:" << std::endl;
    std::cout << "  Topology: " << config.num_rows << "x" << config.num_cols << " mesh" << std::endl;
    std::cout << "  VCs per VN: " << config.virtual_channels_per_vn << std::endl;
    std::cout << "  Link width: " << config.link_width_bytes << " bytes" << std::endl;
    std::cout << "  Router latency: " << config.router_latency << " cycles" << std::endl;

    return config;
}

} // namespace pimid
