/**
 * @file network_config_parser.h
 * @brief Parser for network topology and external model configuration
 */

#ifndef PIMID_NETWORK_CONFIG_PARSER_H
#define PIMID_NETWORK_CONFIG_PARSER_H

#include <string>
#include <yaml-cpp/yaml.h>
#include "memory_models/include/internal_dram_network.h"
#include "external_models/include/external_model_interface.h"

namespace pimid {

/**
 * @brief Configuration for external models
 */
struct ExternalModelConfig {
    bool enabled;
    std::string library_path;
    std::string model_name;
    std::string config_file;

    ExternalModelConfig() : enabled(false) {}
};

/**
 * @brief Combined configuration from YAML file
 */
struct NetworkTopologyConfig {
    // Custom topology settings
    bool custom_topology_enabled;
    SwitchHierarchyConfig switch_config;

    // External model settings
    ExternalModelConfig network_model;
    ExternalModelConfig memory_model;

    NetworkTopologyConfig() : custom_topology_enabled(false) {}
};

/**
 * @brief Parse network topology configuration from YAML
 */
class NetworkConfigParser {
public:
    /**
     * @brief Parse custom topology configuration from YAML node
     *
     * @param config_node YAML node containing network configuration
     * @param num_channels Number of channels (from memory config)
     * @param ranks_per_channel Number of ranks per channel
     * @return SwitchHierarchyConfig parsed configuration
     */
    static SwitchHierarchyConfig parseCustomTopology(
        const YAML::Node& config_node,
        int num_channels,
        int ranks_per_channel);

    /**
     * @brief Parse external model configuration from YAML node
     *
     * @param models_node YAML node containing external_models section
     * @return NetworkTopologyConfig with external model settings
     */
    static ExternalModelConfig parseExternalModel(
        const YAML::Node& model_node);

    /**
     * @brief Parse complete network topology configuration
     *
     * @param config_file Path to YAML configuration file
     * @return NetworkTopologyConfig complete configuration
     */
    static NetworkTopologyConfig parseConfigFile(const std::string& config_file);

    /**
     * @brief Parse topology type from string
     *
     * @param topology_str String representation (e.g., "BUS", "CROSSBAR")
     * @return TopologyType enum value
     */
    static TopologyType parseTopologyType(const std::string& topology_str);

    /**
     * @brief Validate custom topology configuration
     *
     * @param config Configuration to validate
     * @param num_banks_per_bg Expected number of banks per BG
     * @param num_bg_per_chip Expected number of BGs per chip
     * @param num_chips_per_rank Expected number of chips per rank
     * @return true if valid, false otherwise
     */
    static bool validateTopologyConfig(
        const SwitchHierarchyConfig& config,
        int num_banks_per_bg,
        int num_bg_per_chip,
        int num_chips_per_rank);

private:
    /**
     * @brief Parse a single network level configuration
     *
     * @param level_node YAML node for the level (e.g., l0, l1)
     * @param level_num Level number (0-5)
     * @return NetworkLevelConfig parsed level configuration
     */
    static NetworkLevelConfig parseNetworkLevel(
        const YAML::Node& level_node,
        int level_num);
};

} // namespace pimid

#endif // PIMID_NETWORK_CONFIG_PARSER_H
