#ifndef PIMID_CONFIG_PARSER_H
#define PIMID_CONFIG_PARSER_H

#include "common/types.h"
#include "address_translation/pe_placement.h"     // PEDescriptor, MemoryHierarchy
#include "address_translation/address_translator.h"  // AddressTranslationConfig
#include "network/network_model.h"
#include "power/power_model.h"
#include <string>
#include <map>

namespace pimid {

/**
 * YAML-based configuration parser for PIMID
 * Loads and parses configuration files for all components
 */
class ConfigParser {
public:
    ConfigParser();
    ~ConfigParser();

    // Load configuration from YAML file
    bool loadConfig(const std::string& config_file);

    // Get parsed configurations
    PIMIDConfig getPIMIDConfig() const { return pimid_config_; }
    std::vector<HostCoreConfig> getHostCoreConfigs() const { return host_cores_; }
    HostCacheConfig getHostCacheConfig() const { return host_cache_; }
    std::vector<PEDescriptor> getPEDescriptors() const { return pe_descriptors_; }
    MemoryHierarchy getMemoryHierarchy() const { return memory_hierarchy_; }
    NetworkConfig getNetworkConfig() const { return network_config_; }
    TechnologyParams getTechnologyParams() const { return tech_params_; }
    AddressTranslationConfig getAddressTranslationConfig() const { return addr_trans_config_; }

    // Validation
    bool validateConfig() const;
    void printConfig() const;

private:
    // Configuration structures
    PIMIDConfig pimid_config_;
    std::vector<HostCoreConfig> host_cores_;
    HostCacheConfig host_cache_;
    std::vector<PEDescriptor> pe_descriptors_;
    MemoryHierarchy memory_hierarchy_;
    NetworkConfig network_config_;
    TechnologyParams tech_params_;
    AddressTranslationConfig addr_trans_config_;

    // YAML parser instance (use yaml-cpp or similar)
    void* yaml_parser_;

    // Parsing helpers
    void parseHostConfig(void* yaml_node);
    void parseDeviceConfig(void* yaml_node);
    void parseMemoryConfig(void* yaml_node);
    void parseNetworkConfig(void* yaml_node);
    void parsePowerConfig(void* yaml_node);
    void parsePEPlacements(void* yaml_node);

    // Type conversions
    PEPlacementLevel stringToPlacementLevel(const std::string& str) const;
    MemoryTechnology stringToMemoryTech(const std::string& str) const;
    AddressingMode stringToAddressingMode(const std::string& str) const;
    NetworkTopology stringToTopology(const std::string& str) const;
    RoutingAlgorithm stringToRouting(const std::string& str) const;
};

} // namespace pimid

#endif // PIMID_CONFIG_PARSER_H
