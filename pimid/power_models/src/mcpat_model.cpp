#include "power_model.h"
#include <iostream>
#include <iomanip>
#include <cmath>

namespace pimid {

//=============================================================================
// McPATModel Implementation
//=============================================================================

McPATModel::McPATModel(const TechnologyParams& params)
    : PowerModel(params)
    , mcpat_instance_(nullptr) {

    std::cout << "[McPATModel] Creating McPAT-based power model" << std::endl;
}

McPATModel::~McPATModel() {
    // TODO: Clean up McPAT instance when integrated
    // if (mcpat_instance_) {
    //     delete static_cast<McPATWrapper*>(mcpat_instance_);
    // }
}

void McPATModel::initialize() {
    std::cout << "[McPATModel] Initializing McPAT power model..." << std::endl;

    // Initialize default configurations for different components
    initializeDefaultConfigs();

    // TODO: Initialize McPAT instance when integrated
    // mcpat_instance_ = new McPATWrapper(tech_params_);

    std::cout << "[McPATModel] Technology parameters:" << std::endl;
    std::cout << "  Node: " << tech_params_.tech_node_nm << " nm" << std::endl;
    std::cout << "  Device: " << tech_params_.device_type << std::endl;
    std::cout << "  Temperature: " << tech_params_.temperature_k << " K" << std::endl;
    std::cout << "  Frequency: " << tech_params_.frequency_ghz << " GHz" << std::endl;

    std::cout << "[McPATModel] Initialization complete" << std::endl;
}

void McPATModel::loadConfig(const std::string& config_path) {
    // TODO: Parse YAML configuration file
    // For now, use default configuration
    std::cout << "[McPATModel] Loading configuration from: " << config_path << std::endl;
    std::cout << "[McPATModel] Using default McPAT configuration" << std::endl;
}

PowerMetrics McPATModel::estimatePower(PowerComponent component,
                                        const ActivityStats& stats) {
    PowerMetrics metrics;

    // Calculate power based on component type and activity
    switch (component) {
        case PowerComponent::CORE:
            metrics = estimateCorePower(stats);
            break;

        case PowerComponent::L1_CACHE:
        case PowerComponent::L2_CACHE:
        case PowerComponent::L3_CACHE:
            metrics = estimateCachePower(component, stats);
            break;

        case PowerComponent::MEMORY_CONTROLLER:
            metrics = estimateMemoryControllerPower(stats);
            break;

        case PowerComponent::PE:
            metrics = estimatePEPower(stats);
            break;

        case PowerComponent::NETWORK_ROUTER:
        case PowerComponent::NETWORK_LINK:
        case PowerComponent::MEMORY:
            // These are handled by specialized models
            break;
    }

    // Update stored metrics
    component_metrics_[component] = metrics;
    activity_stats_[component] = stats;

    return metrics;
}

double McPATModel::getDynamicPower(PowerComponent component) const {
    auto it = component_metrics_.find(component);
    if (it != component_metrics_.end()) {
        return it->second.dynamic_power_w;
    }
    return 0.0;
}

double McPATModel::getLeakagePower(PowerComponent component) const {
    auto it = component_metrics_.find(component);
    if (it != component_metrics_.end()) {
        return it->second.leakage_power_w;
    }
    return 0.0;
}

double McPATModel::getTotalPower() const {
    double total = 0.0;
    for (const auto& pair : component_metrics_) {
        total += pair.second.total_power_w;
    }
    return total;
}

double McPATModel::getEnergy(PowerComponent component) const {
    auto it = component_metrics_.find(component);
    if (it != component_metrics_.end()) {
        return it->second.total_energy_j;
    }
    return 0.0;
}

double McPATModel::getTotalEnergy() const {
    double total = 0.0;
    for (const auto& pair : component_metrics_) {
        total += pair.second.total_energy_j;
    }
    return total;
}

void McPATModel::updateActivity(PowerComponent component,
                                 const ActivityStats& stats) {
    activity_stats_[component] = stats;

    // Re-estimate power with new activity
    auto metrics = estimatePower(component, stats);

    // Accumulate energy
    double time_s = static_cast<double>(stats.total_cycles) /
                   (tech_params_.frequency_ghz * 1e9);
    component_metrics_[component].total_energy_j +=
        metrics.total_power_w * time_s;
}

void McPATModel::printStats() const {
    std::cout << "\n=== McPAT Power Model Statistics ===" << std::endl;

    std::cout << std::fixed << std::setprecision(3);

    for (const auto& pair : component_metrics_) {
        std::cout << "\nComponent: " << componentToString(pair.first) << std::endl;
        std::cout << "  Dynamic Power: " << pair.second.dynamic_power_w << " W" << std::endl;
        std::cout << "  Leakage Power: " << pair.second.leakage_power_w << " W" << std::endl;
        std::cout << "  Total Power: " << pair.second.total_power_w << " W" << std::endl;
        std::cout << "  Total Energy: " << pair.second.total_energy_j << " J" << std::endl;

        // Print activity stats if available
        auto stats_it = activity_stats_.find(pair.first);
        if (stats_it != activity_stats_.end()) {
            const auto& stats = stats_it->second;
            std::cout << "  Activity:" << std::endl;
            std::cout << "    Total Cycles: " << stats.total_cycles << std::endl;
            std::cout << "    Total Instructions: " << stats.total_instructions << std::endl;
            if (stats.total_cycles > 0) {
                double ipc = static_cast<double>(stats.total_instructions) / stats.total_cycles;
                std::cout << "    IPC: " << ipc << std::endl;
            }
        }
    }

    std::cout << "\n--- Summary ---" << std::endl;
    std::cout << "Total Power: " << getTotalPower() << " W" << std::endl;
    std::cout << "Total Energy: " << getTotalEnergy() << " J" << std::endl;
    std::cout << "=====================================\n" << std::endl;
}

void McPATModel::resetStats() {
    component_metrics_.clear();
    activity_stats_.clear();
}

//=============================================================================
// Private Helper Functions
//=============================================================================

void McPATModel::initializeDefaultConfigs() {
    // Default core configuration (out-of-order)
    CoreConfig core_cfg;
    core_cfg.pipeline_depth = 14;
    core_cfg.issue_width = 4;
    core_cfg.alu_count = 4;
    core_cfg.mul_count = 2;
    core_cfg.fpu_count = 2;
    core_configs_[PowerComponent::CORE] = core_cfg;
    core_configs_[PowerComponent::PE] = core_cfg;  // PEs use similar config

    // Cache configurations
    CacheConfig l1_cfg;
    l1_cfg.size_bytes = 32 * 1024;  // 32 KB
    l1_cfg.line_size = 64;
    l1_cfg.associativity = 8;
    l1_cfg.banks = 2;
    l1_cfg.replacement_policy = "LRU";
    cache_configs_[PowerComponent::L1_CACHE] = l1_cfg;

    CacheConfig l2_cfg;
    l2_cfg.size_bytes = 256 * 1024;  // 256 KB
    l2_cfg.line_size = 64;
    l2_cfg.associativity = 8;
    l2_cfg.banks = 4;
    l2_cfg.replacement_policy = "LRU";
    cache_configs_[PowerComponent::L2_CACHE] = l2_cfg;

    CacheConfig l3_cfg;
    l3_cfg.size_bytes = 8 * 1024 * 1024;  // 8 MB
    l3_cfg.line_size = 64;
    l3_cfg.associativity = 16;
    l3_cfg.banks = 16;
    l3_cfg.replacement_policy = "LRU";
    cache_configs_[PowerComponent::L3_CACHE] = l3_cfg;
}

PowerMetrics McPATModel::estimateCorePower(const ActivityStats& stats) {
    PowerMetrics metrics;

    // Technology-dependent base power (simplified model)
    double tech_factor = 45.0 / tech_params_.tech_node_nm;  // Normalized to 45nm
    double freq_factor = tech_params_.frequency_ghz / 2.0;   // Normalized to 2 GHz

    // Calculate dynamic power based on activity
    double utilization = 0.0;
    if (stats.total_cycles > 0) {
        utilization = static_cast<double>(stats.total_instructions) / stats.total_cycles;
        utilization = std::min(utilization, 4.0) / 4.0;  // Normalize to issue width
    }

    // Base dynamic power for active core (typical 22nm @ 2GHz)
    double base_dynamic = 5.0 * tech_factor * freq_factor;
    metrics.dynamic_power_w = base_dynamic * utilization;

    // Leakage power (scales with temperature)
    double temp_factor = std::exp((tech_params_.temperature_k - 350.0) / 50.0);
    metrics.leakage_power_w = 1.0 * tech_factor * temp_factor;

    metrics.total_power_w = metrics.dynamic_power_w + metrics.leakage_power_w;

    return metrics;
}

PowerMetrics McPATModel::estimateCachePower(PowerComponent component,
                                              const ActivityStats& stats) {
    PowerMetrics metrics;

    auto it = cache_configs_.find(component);
    if (it == cache_configs_.end()) {
        return metrics;
    }

    const auto& cfg = it->second;

    // Technology scaling
    double tech_factor = 45.0 / tech_params_.tech_node_nm;

    // Calculate accesses based on component
    uint64_t reads = 0, writes = 0;
    switch (component) {
        case PowerComponent::L1_CACHE:
            reads = stats.l1_reads;
            writes = stats.l1_writes;
            break;
        case PowerComponent::L2_CACHE:
            reads = stats.l2_reads;
            writes = stats.l2_writes;
            break;
        case PowerComponent::L3_CACHE:
            reads = stats.memory_reads;
            writes = stats.memory_writes;
            break;
        default:
            break;
    }

    // Energy per access (simplified CACTI-based model)
    double size_mb = cfg.size_bytes / (1024.0 * 1024.0);
    double read_energy_nj = 0.1 * size_mb * tech_factor;
    double write_energy_nj = 0.15 * size_mb * tech_factor;

    // Dynamic power = (energy per access * accesses) / time
    if (stats.total_cycles > 0) {
        double time_s = stats.total_cycles / (tech_params_.frequency_ghz * 1e9);
        double total_energy_j = (reads * read_energy_nj + writes * write_energy_nj) * 1e-9;
        metrics.dynamic_power_w = total_energy_j / time_s;
    }

    // Leakage power (scales with cache size)
    metrics.leakage_power_w = 0.05 * size_mb * tech_factor;

    metrics.total_power_w = metrics.dynamic_power_w + metrics.leakage_power_w;

    return metrics;
}

PowerMetrics McPATModel::estimateMemoryControllerPower(const ActivityStats& stats) {
    PowerMetrics metrics;

    double tech_factor = 45.0 / tech_params_.tech_node_nm;

    // Dynamic power based on memory traffic
    uint64_t total_accesses = stats.memory_reads + stats.memory_writes;
    if (stats.total_cycles > 0 && total_accesses > 0) {
        double access_rate = static_cast<double>(total_accesses) / stats.total_cycles;
        metrics.dynamic_power_w = 2.0 * access_rate * tech_factor;
    }

    // Leakage power
    metrics.leakage_power_w = 0.5 * tech_factor;

    metrics.total_power_w = metrics.dynamic_power_w + metrics.leakage_power_w;

    return metrics;
}

PowerMetrics McPATModel::estimatePEPower(const ActivityStats& stats) {
    // Processing elements use similar model to cores but may be simpler
    PowerMetrics metrics = estimateCorePower(stats);

    // PEs might have lower complexity
    metrics.dynamic_power_w *= 0.7;  // 70% of full core
    metrics.leakage_power_w *= 0.7;
    metrics.total_power_w = metrics.dynamic_power_w + metrics.leakage_power_w;

    return metrics;
}

void McPATModel::generateMcPATInput(PowerComponent component,
                                     const ActivityStats& stats) {
    // TODO: Generate XML input file for McPAT
    // This will be implemented when McPAT integration is complete
}

PowerMetrics McPATModel::parseMcPATOutput() {
    // TODO: Parse McPAT XML output
    // This will be implemented when McPAT integration is complete
    return PowerMetrics();
}

double McPATModel::calculateComponentEnergy(PowerComponent component, Cycle cycles) {
    auto it = component_metrics_.find(component);
    if (it != component_metrics_.end()) {
        double time_s = cycles / (tech_params_.frequency_ghz * 1e9);
        return it->second.total_power_w * time_s;
    }
    return 0.0;
}

std::string McPATModel::componentToString(PowerComponent component) const {
    switch (component) {
        case PowerComponent::CORE: return "Core";
        case PowerComponent::L1_CACHE: return "L1 Cache";
        case PowerComponent::L2_CACHE: return "L2 Cache";
        case PowerComponent::L3_CACHE: return "L3 Cache";
        case PowerComponent::MEMORY_CONTROLLER: return "Memory Controller";
        case PowerComponent::MEMORY: return "Memory";
        case PowerComponent::NETWORK_ROUTER: return "Network Router";
        case PowerComponent::NETWORK_LINK: return "Network Link";
        case PowerComponent::PE: return "Processing Element";
        default: return "Unknown";
    }
}

} // namespace pimid
