#include "power/power_model_manager.h"
#include "network/network_model.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>

// Forward declarations for optional specialized models
namespace pimid {
    class RamulatorWrapper;
}

// Ramulator wrapper is optional - only include if available
#ifdef HAVE_RAMULATOR_WRAPPER
#include "memory/ramulator_wrapper.h"
#endif

namespace pimid {

//=============================================================================
// PowerModelManager Implementation
//=============================================================================

PowerModelManager::PowerModelManager(const TechnologyParams& params)
    : tech_params_(params)
    , mcpat_fallback_enabled_(true)
    , analytical_fallback_enabled_(true)
    , initialized_(false) {

    std::cout << "[PowerModelManager] Creating hierarchical power model manager"
              << std::endl;
    std::cout << "  Technology: " << params.tech_node_nm << " nm" << std::endl;
    std::cout << "  Frequency: " << params.frequency_ghz << " GHz" << std::endl;
    std::cout << "  Temperature: " << params.temperature_k << " K" << std::endl;
}

PowerModelManager::~PowerModelManager() = default;

void PowerModelManager::initialize() {
    if (initialized_) {
        std::cerr << "[PowerModelManager] Warning: Already initialized" << std::endl;
        return;
    }

    std::cout << "[PowerModelManager] Initializing power model hierarchy..."
              << std::endl;

    // Initialize McPAT wrapper (guaranteed fallback)
    if (mcpat_fallback_enabled_) {
        std::cout << "  Setting up McPAT fallback..." << std::endl;

        // Configure McPAT with technology parameters
        mcpat_config_.num_cores = tech_params_.core_count;
        mcpat_config_.core_clock_mhz = tech_params_.frequency_ghz * 1000.0;
        mcpat_config_.tech_node_nm = tech_params_.tech_node_nm;
        mcpat_config_.temperature_k = tech_params_.temperature_k;

        // Default cache sizes (can be overridden via config)
        mcpat_config_.l1i_size_bytes = 32 * 1024;
        mcpat_config_.l1d_size_bytes = 32 * 1024;
        mcpat_config_.l2_size_bytes = 256 * 1024;
        mcpat_config_.l3_size_bytes = 8 * 1024 * 1024;

        // Memory controllers and NoC
        mcpat_config_.num_memory_controllers = 1;
        mcpat_config_.has_noc = true;

        // Create McPAT wrapper
        mcpat_wrapper_ = std::make_shared<McPATWrapper>(mcpat_config_);
        mcpat_wrapper_->initialize();

        std::cout << "  ✓ McPAT initialized successfully" << std::endl;
    }

    // Initialize specialized models (if registered)
    if (ramulator_model_) {
        std::cout << "  ✓ Ramulator model registered for MEMORY" << std::endl;
    }

    if (garnet_model_) {
        std::cout << "  ✓ GARNET model registered for NETWORK" << std::endl;
    }

    if (!custom_models_.empty()) {
        std::cout << "  ✓ " << custom_models_.size()
                  << " custom model(s) registered" << std::endl;
    }

    initialized_ = true;
    std::cout << "[PowerModelManager] Initialization complete" << std::endl;

    // Print coverage report
    printCoverageReport();
}

void PowerModelManager::loadConfig(const std::string& config_path) {
    std::cout << "[PowerModelManager] Loading configuration from: "
              << config_path << std::endl;

    try {
        YAML::Node config = YAML::LoadFile(config_path);

        // Parse technology parameters section
        if (config["technology"]) {
            auto tech = config["technology"];
            if (tech["node_nm"]) {
                tech_params_.tech_node_nm = tech["node_nm"].as<double>();
            }
            if (tech["frequency_ghz"]) {
                tech_params_.frequency_ghz = tech["frequency_ghz"].as<double>();
            }
            if (tech["temperature_k"]) {
                tech_params_.temperature_k = tech["temperature_k"].as<double>();
            }
            if (tech["supply_voltage_v"]) {
                tech_params_.supply_voltage_v = tech["supply_voltage_v"].as<double>();
            }
            if (tech["core_count"]) {
                tech_params_.core_count = tech["core_count"].as<uint32_t>();
            }
        }

        // Parse McPAT configuration section
        if (config["mcpat"]) {
            auto mcpat = config["mcpat"];

            if (mcpat["enabled"]) {
                mcpat_fallback_enabled_ = mcpat["enabled"].as<bool>();
            }

            // Core parameters
            if (mcpat["cores"]) {
                mcpat_config_.num_cores = mcpat["cores"].as<uint32_t>();
            }
            if (mcpat["core_clock_mhz"]) {
                mcpat_config_.core_clock_mhz = mcpat["core_clock_mhz"].as<double>();
            }

            // Cache sizes
            if (mcpat["l1i_size_kb"]) {
                mcpat_config_.l1i_size_bytes = mcpat["l1i_size_kb"].as<uint64_t>() * 1024;
            }
            if (mcpat["l1d_size_kb"]) {
                mcpat_config_.l1d_size_bytes = mcpat["l1d_size_kb"].as<uint64_t>() * 1024;
            }
            if (mcpat["l2_size_kb"]) {
                mcpat_config_.l2_size_bytes = mcpat["l2_size_kb"].as<uint64_t>() * 1024;
            }
            if (mcpat["l3_size_mb"]) {
                mcpat_config_.l3_size_bytes = mcpat["l3_size_mb"].as<uint64_t>() * 1024 * 1024;
            }

            // Memory controllers
            if (mcpat["memory_controllers"]) {
                mcpat_config_.num_memory_controllers = mcpat["memory_controllers"].as<uint32_t>();
            }
            if (mcpat["has_noc"]) {
                mcpat_config_.has_noc = mcpat["has_noc"].as<bool>();
            }
        }

        // Parse analytical model configuration
        if (config["analytical"]) {
            auto analytical = config["analytical"];

            if (analytical["enabled"]) {
                analytical_fallback_enabled_ = analytical["enabled"].as<bool>();
            }

            // Component-specific scaling factors
            if (analytical["core_power_scale"]) {
                // Could store scaling factors for analytical model tuning
                std::cout << "  Core power scaling: "
                          << analytical["core_power_scale"].as<double>() << "x" << std::endl;
            }
            if (analytical["memory_power_scale"]) {
                std::cout << "  Memory power scaling: "
                          << analytical["memory_power_scale"].as<double>() << "x" << std::endl;
            }
        }

        // Parse power budget constraints
        if (config["power_budget"]) {
            auto budget = config["power_budget"];
            if (budget["max_total_w"]) {
                std::cout << "  Power budget: "
                          << budget["max_total_w"].as<double>() << " W" << std::endl;
            }
            if (budget["thermal_limit_w"]) {
                std::cout << "  Thermal limit: "
                          << budget["thermal_limit_w"].as<double>() << " W" << std::endl;
            }
        }

        // Parse specialized model paths
        if (config["specialized_models"]) {
            auto models = config["specialized_models"];

            if (models["ramulator_config"]) {
                std::cout << "  Ramulator config: "
                          << models["ramulator_config"].as<std::string>() << std::endl;
            }
            if (models["garnet_config"]) {
                std::cout << "  GARNET config: "
                          << models["garnet_config"].as<std::string>() << std::endl;
            }
        }

        std::cout << "[PowerModelManager] Configuration loaded successfully" << std::endl;
        std::cout << "  Technology: " << tech_params_.tech_node_nm << " nm" << std::endl;
        std::cout << "  Frequency: " << tech_params_.frequency_ghz << " GHz" << std::endl;
        std::cout << "  Temperature: " << tech_params_.temperature_k << " K" << std::endl;
        std::cout << "  McPAT: " << (mcpat_fallback_enabled_ ? "enabled" : "disabled") << std::endl;
        std::cout << "  Analytical: " << (analytical_fallback_enabled_ ? "enabled" : "disabled") << std::endl;

    } catch (const YAML::BadFile& e) {
        std::cerr << "[PowerModelManager] ERROR: Cannot open config file: "
                  << config_path << std::endl;
    } catch (const YAML::ParserException& e) {
        std::cerr << "[PowerModelManager] ERROR: YAML parsing error in "
                  << config_path << ": " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[PowerModelManager] ERROR: Failed to load config: "
                  << e.what() << std::endl;
    }
}

//=============================================================================
// Register Specialized Models
//=============================================================================

void PowerModelManager::setRamulatorModel(std::shared_ptr<RamulatorWrapper> ramulator) {
    ramulator_model_ = ramulator;
    std::cout << "[PowerModelManager] Registered Ramulator for MEMORY power"
              << std::endl;
}

void PowerModelManager::setGarnetModel(std::shared_ptr<GarnetModel> garnet) {
    garnet_model_ = garnet;
    std::cout << "[PowerModelManager] Registered GARNET for NETWORK power"
              << std::endl;
}

void PowerModelManager::registerCustomModel(PowerComponent component,
                                            CustomPowerFunc func,
                                            const std::string& name) {
    custom_models_[component] = func;
    custom_model_names_[component] = name;
    std::cout << "[PowerModelManager] Registered custom model '" << name
              << "' for " << componentName(component) << std::endl;
}

//=============================================================================
// Power Estimation with Hierarchical Fallback
//=============================================================================

PowerEstimate PowerModelManager::getPower(PowerComponent component,
                                         const ActivityStats& stats) {
    if (!initialized_) {
        std::cerr << "[PowerModelManager] ERROR: Not initialized!" << std::endl;
        return PowerEstimate();
    }

    // Cache activity stats
    activity_stats_[component] = stats;

    // Try hierarchical fallback
    PowerEstimate estimate;

    // 1. Try specialized model first (highest priority)
    estimate = trySpecializedModel(component, stats);
    if (estimate.is_valid) {
        usage_stats_[component] = PowerModelSource::SPECIALIZED_SIMULATOR;
        cached_power_[component] = estimate;
        return estimate;
    }

    // 2. Try McPAT (guaranteed fallback)
    if (mcpat_fallback_enabled_) {
        estimate = tryMcPAT(component, stats);
        if (estimate.is_valid) {
            usage_stats_[component] = PowerModelSource::MCPAT;
            cached_power_[component] = estimate;
            return estimate;
        }
    }

    // 3. Analytical fallback (always works)
    if (analytical_fallback_enabled_) {
        estimate = analyticalFallback(component, stats);
        usage_stats_[component] = PowerModelSource::ANALYTICAL;
        cached_power_[component] = estimate;
        return estimate;
    }

    // Should never get here
    std::cerr << "[PowerModelManager] ERROR: All power models failed for "
              << componentName(component) << std::endl;
    return PowerEstimate();
}

void PowerModelManager::updateActivity(PowerComponent component,
                                       const ActivityStats& stats) {
    // Re-estimate power with new activity
    getPower(component, stats);
}

//=============================================================================
// Try Specialized Models
//=============================================================================

PowerEstimate PowerModelManager::trySpecializedModel(PowerComponent component,
                                                     const ActivityStats& stats) {
    // Check custom models first
    auto custom_it = custom_models_.find(component);
    if (custom_it != custom_models_.end()) {
        try {
            PowerEstimate estimate = custom_it->second(stats);
            if (estimate.is_valid) {
                estimate.source_name = custom_model_names_[component];
                return estimate;
            }
        } catch (const std::exception& e) {
            std::cerr << "[PowerModelManager] Custom model failed: "
                      << e.what() << std::endl;
        }
    }

    // Component-specific specialized models
    switch (component) {
        case PowerComponent::MEMORY:
            if (ramulator_model_) {
                return extractRamulatorPower(stats);
            }
            break;

        case PowerComponent::NETWORK_ROUTER:
        case PowerComponent::NETWORK_LINK:
            if (garnet_model_) {
                return extractGarnetPower(component, stats);
            }
            break;

        default:
            break;
    }

    // No specialized model available
    return PowerEstimate();
}

PowerEstimate PowerModelManager::extractRamulatorPower(const ActivityStats& stats) {
    if (!ramulator_model_) {
        return PowerEstimate();
    }

#ifdef HAVE_RAMULATOR_WRAPPER
    try {
        // Get energy from Ramulator
        double total_energy_j = ramulator_model_->getTotalEnergy();

        // Calculate power from energy and time
        double time_s = stats.total_cycles / (tech_params_.frequency_ghz * 1e9);
        double power_w = (time_s > 0) ? (total_energy_j / time_s) : 0.0;

        PowerMetrics metrics;
        metrics.total_power_w = power_w;
        metrics.total_energy_j = total_energy_j;
        // Ramulator provides total power, split 80% dynamic / 20% leakage (typical)
        metrics.dynamic_power_w = power_w * 0.8;
        metrics.leakage_power_w = power_w * 0.2;

        return PowerEstimate(metrics, PowerModelSource::SPECIALIZED_SIMULATOR, "Ramulator");

    } catch (const std::exception& e) {
        std::cerr << "[PowerModelManager] Ramulator power extraction failed: "
                  << e.what() << std::endl;
        return PowerEstimate();
    }
#else
    // Ramulator wrapper not available - return empty estimate
    // This will trigger fallback to McPAT or analytical models
    (void)stats;  // Suppress unused parameter warning
    return PowerEstimate();
#endif
}

PowerEstimate PowerModelManager::extractGarnetPower(PowerComponent component,
                                                    const ActivityStats& stats) {
    if (!garnet_model_) {
        return PowerEstimate();
    }

    try {
        PowerMetrics metrics;

        // Get energy from GARNET
        if (component == PowerComponent::NETWORK_ROUTER) {
            double energy_j = garnet_model_->getRouterEnergy();
            double time_s = stats.total_cycles / (tech_params_.frequency_ghz * 1e9);
            metrics.total_power_w = (time_s > 0) ? (energy_j / time_s) : 0.0;
            metrics.total_energy_j = energy_j;
        } else if (component == PowerComponent::NETWORK_LINK) {
            double energy_j = garnet_model_->getLinkEnergy();
            double time_s = stats.total_cycles / (tech_params_.frequency_ghz * 1e9);
            metrics.total_power_w = (time_s > 0) ? (energy_j / time_s) : 0.0;
            metrics.total_energy_j = energy_j;
        }

        // GARNET provides dynamic energy, assume 10% leakage
        metrics.dynamic_power_w = metrics.total_power_w * 0.9;
        metrics.leakage_power_w = metrics.total_power_w * 0.1;

        return PowerEstimate(metrics, PowerModelSource::SPECIALIZED_SIMULATOR, "GARNET");

    } catch (const std::exception& e) {
        std::cerr << "[PowerModelManager] GARNET power extraction failed: "
                  << e.what() << std::endl;
        return PowerEstimate();
    }
}

//=============================================================================
// Try McPAT
//=============================================================================

PowerEstimate PowerModelManager::tryMcPAT(PowerComponent component,
                                         const ActivityStats& stats) {
    if (!mcpat_wrapper_) {
        return PowerEstimate();
    }

    try {
        // Update McPAT with activity statistics
        mcpat_wrapper_->setTotalCycles(stats.total_cycles);
        mcpat_wrapper_->setTotalInstructions(stats.total_instructions);
        // Approximate: treat all L1 reads/writes as L1D, estimate 10% miss rate
        mcpat_wrapper_->setL1IAccesses(stats.l1_reads / 4, stats.l1_reads / 40);
        mcpat_wrapper_->setL1DAccesses(stats.l1_reads, stats.l1_writes,
                                        stats.l1_reads / 10, stats.l1_writes / 10);
        mcpat_wrapper_->setL2Accesses(stats.l2_reads, stats.l2_writes,
                                       stats.l2_reads / 10, stats.l2_writes / 10);
        mcpat_wrapper_->setL3Accesses(stats.memory_reads, stats.memory_writes,
                                       stats.memory_reads / 10, stats.memory_writes / 10);

        // Compute power
        mcpat_wrapper_->computePower();

        // Map our component to McPAT component
        auto mcpat_comp = toMcPATComponent(component);

        // Get power metrics from McPAT
        auto mcpat_metrics = mcpat_wrapper_->getComponentPower(mcpat_comp);

        // Convert to our PowerMetrics format
        PowerMetrics metrics;
        metrics.dynamic_power_w = mcpat_metrics.runtime_dynamic;
        metrics.leakage_power_w = mcpat_metrics.total_leakage;
        metrics.total_power_w = mcpat_metrics.total_power;
        metrics.total_energy_j = mcpat_metrics.total_energy;

        return PowerEstimate(metrics, PowerModelSource::MCPAT, "McPAT");

    } catch (const std::exception& e) {
        std::cerr << "[PowerModelManager] McPAT power estimation failed: "
                  << e.what() << std::endl;
        return PowerEstimate();
    }
}

McPATWrapper::ComponentType PowerModelManager::toMcPATComponent(PowerComponent component) const {
    switch (component) {
        case PowerComponent::CORE:
            return McPATWrapper::ComponentType::CORE;
        case PowerComponent::L1_CACHE:
            return McPATWrapper::ComponentType::L1_CACHE;
        case PowerComponent::L2_CACHE:
            return McPATWrapper::ComponentType::L2_CACHE;
        case PowerComponent::L3_CACHE:
            return McPATWrapper::ComponentType::L3_CACHE;
        case PowerComponent::MEMORY_CONTROLLER:
            return McPATWrapper::ComponentType::MEMORY_CONTROLLER;
        case PowerComponent::NETWORK_ROUTER:
        case PowerComponent::NETWORK_LINK:
            return McPATWrapper::ComponentType::NOC;
        default:
            return McPATWrapper::ComponentType::CORE;  // Default fallback
    }
}

//=============================================================================
// Analytical Fallback
//=============================================================================

PowerEstimate PowerModelManager::analyticalFallback(PowerComponent component,
                                                   const ActivityStats& stats) {
    // Simple analytical power models (guaranteed to work)
    PowerMetrics metrics;

    double tech_factor = 45.0 / tech_params_.tech_node_nm;
    double freq_factor = tech_params_.frequency_ghz / 2.0;

    switch (component) {
        case PowerComponent::CORE: {
            double utilization = (stats.total_cycles > 0) ?
                std::min(1.0, static_cast<double>(stats.total_instructions) / stats.total_cycles) : 0.0;
            metrics.dynamic_power_w = 5.0 * tech_factor * freq_factor * utilization;
            metrics.leakage_power_w = 1.0 * tech_factor;
            break;
        }

        case PowerComponent::L1_CACHE: {
            uint64_t accesses = stats.l1_reads + stats.l1_writes;
            double access_rate = (stats.total_cycles > 0) ?
                static_cast<double>(accesses) / stats.total_cycles : 0.0;
            metrics.dynamic_power_w = 0.3 * access_rate * tech_factor;
            metrics.leakage_power_w = 0.1 * tech_factor;
            break;
        }

        case PowerComponent::L2_CACHE: {
            uint64_t accesses = stats.l2_reads + stats.l2_writes;
            double access_rate = (stats.total_cycles > 0) ?
                static_cast<double>(accesses) / stats.total_cycles : 0.0;
            metrics.dynamic_power_w = 0.5 * access_rate * tech_factor;
            metrics.leakage_power_w = 0.2 * tech_factor;
            break;
        }

        case PowerComponent::L3_CACHE: {
            uint64_t accesses = stats.memory_reads + stats.memory_writes;
            double access_rate = (stats.total_cycles > 0) ?
                static_cast<double>(accesses) / stats.total_cycles : 0.0;
            metrics.dynamic_power_w = 0.8 * access_rate * tech_factor;
            metrics.leakage_power_w = 0.4 * tech_factor;
            break;
        }

        case PowerComponent::MEMORY_CONTROLLER: {
            uint64_t accesses = stats.memory_reads + stats.memory_writes;
            double access_rate = (stats.total_cycles > 0) ?
                static_cast<double>(accesses) / stats.total_cycles : 0.0;
            metrics.dynamic_power_w = 2.0 * access_rate * tech_factor;
            metrics.leakage_power_w = 0.5 * tech_factor;
            break;
        }

        case PowerComponent::MEMORY: {
            // DDR4 typical: ~3W per DIMM at full load
            uint64_t accesses = stats.memory_reads + stats.memory_writes;
            double utilization = (stats.total_cycles > 0) ?
                std::min(1.0, static_cast<double>(accesses) / stats.total_cycles * 100.0) : 0.0;
            metrics.dynamic_power_w = 3.0 * utilization;
            metrics.leakage_power_w = 0.5;
            break;
        }

        case PowerComponent::NETWORK_ROUTER: {
            metrics.dynamic_power_w = 0.5 * tech_factor;
            metrics.leakage_power_w = 0.1 * tech_factor;
            break;
        }

        case PowerComponent::NETWORK_LINK: {
            metrics.dynamic_power_w = 0.2 * tech_factor;
            metrics.leakage_power_w = 0.05 * tech_factor;
            break;
        }

        case PowerComponent::PE: {
            double utilization = (stats.total_cycles > 0) ?
                std::min(1.0, static_cast<double>(stats.total_instructions) / stats.total_cycles) : 0.0;
            metrics.dynamic_power_w = 3.0 * tech_factor * freq_factor * utilization;
            metrics.leakage_power_w = 0.7 * tech_factor;
            break;
        }
    }

    metrics.total_power_w = metrics.dynamic_power_w + metrics.leakage_power_w;

    // Calculate energy
    double time_s = stats.total_cycles / (tech_params_.frequency_ghz * 1e9);
    metrics.total_energy_j = metrics.total_power_w * time_s;

    return PowerEstimate(metrics, PowerModelSource::ANALYTICAL, "Analytical");
}

//=============================================================================
// Query Functions
//=============================================================================

bool PowerModelManager::hasSpecializedModel(PowerComponent component) const {
    // Check custom models
    if (custom_models_.find(component) != custom_models_.end()) {
        return true;
    }

    // Check component-specific specialized models
    switch (component) {
        case PowerComponent::MEMORY:
            return (ramulator_model_ != nullptr);

        case PowerComponent::NETWORK_ROUTER:
        case PowerComponent::NETWORK_LINK:
            return (garnet_model_ != nullptr);

        default:
            return false;
    }
}

PowerModelSource PowerModelManager::getPowerSource(PowerComponent component) const {
    if (hasSpecializedModel(component)) {
        return PowerModelSource::SPECIALIZED_SIMULATOR;
    } else if (mcpat_fallback_enabled_ && mcpat_wrapper_) {
        return PowerModelSource::MCPAT;
    } else if (analytical_fallback_enabled_) {
        return PowerModelSource::ANALYTICAL;
    }
    return PowerModelSource::UNKNOWN;
}

double PowerModelManager::getTotalPower() const {
    double total = 0.0;
    for (const auto& pair : cached_power_) {
        total += pair.second.metrics.total_power_w;
    }
    return total;
}

double PowerModelManager::getTotalEnergy() const {
    double total = 0.0;
    for (const auto& pair : cached_power_) {
        total += pair.second.metrics.total_energy_j;
    }
    return total;
}

PowerModelManager::PowerBreakdown PowerModelManager::getPowerBreakdown() const {
    PowerBreakdown breakdown;
    breakdown.specialized_power_w = 0.0;
    breakdown.mcpat_power_w = 0.0;
    breakdown.analytical_power_w = 0.0;
    breakdown.total_power_w = 0.0;

    for (const auto& pair : cached_power_) {
        const auto& estimate = pair.second;
        double power = estimate.metrics.total_power_w;

        breakdown.total_power_w += power;

        switch (estimate.source) {
            case PowerModelSource::SPECIALIZED_SIMULATOR:
                breakdown.specialized_power_w += power;
                breakdown.source_breakdown[estimate.source_name] += power;
                break;

            case PowerModelSource::MCPAT:
                breakdown.mcpat_power_w += power;
                breakdown.source_breakdown["McPAT"] += power;
                break;

            case PowerModelSource::ANALYTICAL:
                breakdown.analytical_power_w += power;
                breakdown.source_breakdown["Analytical"] += power;
                break;

            default:
                break;
        }
    }

    return breakdown;
}

//=============================================================================
// Statistics and Debugging
//=============================================================================

void PowerModelManager::printStats() const {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗"
              << std::endl;
    std::cout << "║      Hierarchical Power Model Manager Statistics         ║"
              << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝"
              << std::endl;

    std::cout << std::fixed << std::setprecision(3);

    // Print per-component power with sources
    std::cout << "\nPower by Component:\n" << std::endl;
    std::cout << std::setw(20) << "Component"
              << std::setw(12) << "Dynamic (W)"
              << std::setw(12) << "Leakage (W)"
              << std::setw(12) << "Total (W)"
              << std::setw(20) << "Source"
              << std::endl;
    std::cout << std::string(76, '-') << std::endl;

    for (const auto& pair : cached_power_) {
        const auto& estimate = pair.second;
        std::cout << std::setw(20) << componentName(pair.first)
                  << std::setw(12) << estimate.metrics.dynamic_power_w
                  << std::setw(12) << estimate.metrics.leakage_power_w
                  << std::setw(12) << estimate.metrics.total_power_w
                  << std::setw(20) << estimate.source_name
                  << std::endl;
    }

    std::cout << std::string(76, '-') << std::endl;
    std::cout << std::setw(44) << "TOTAL:"
              << std::setw(12) << getTotalPower()
              << std::endl;

    // Print power breakdown by source
    auto breakdown = getPowerBreakdown();
    std::cout << "\nPower Breakdown by Source:\n" << std::endl;
    std::cout << "  Specialized Simulators: " << breakdown.specialized_power_w << " W";
    if (breakdown.specialized_power_w > 0) {
        std::cout << " (" << (breakdown.specialized_power_w / breakdown.total_power_w * 100.0)
                  << "%)";
    }
    std::cout << std::endl;

    std::cout << "  McPAT:                  " << breakdown.mcpat_power_w << " W";
    if (breakdown.mcpat_power_w > 0) {
        std::cout << " (" << (breakdown.mcpat_power_w / breakdown.total_power_w * 100.0)
                  << "%)";
    }
    std::cout << std::endl;

    std::cout << "  Analytical Models:      " << breakdown.analytical_power_w << " W";
    if (breakdown.analytical_power_w > 0) {
        std::cout << " (" << (breakdown.analytical_power_w / breakdown.total_power_w * 100.0)
                  << "%)";
    }
    std::cout << std::endl;

    std::cout << "  TOTAL:                  " << breakdown.total_power_w << " W"
              << std::endl;

    // Detailed source breakdown
    if (!breakdown.source_breakdown.empty()) {
        std::cout << "\nDetailed Source Breakdown:\n" << std::endl;
        for (const auto& src_pair : breakdown.source_breakdown) {
            double percentage = (src_pair.second / breakdown.total_power_w) * 100.0;
            std::cout << "  " << std::setw(20) << src_pair.first << ": "
                      << std::setw(8) << src_pair.second << " W ("
                      << std::setprecision(1) << percentage << "%)"
                      << std::endl;
        }
    }

    std::cout << "\nTotal Energy: " << std::setprecision(6)
              << getTotalEnergy() * 1e6 << " μJ" << std::endl;
    std::cout << std::string(76, '=') << "\n" << std::endl;
}

void PowerModelManager::printCoverageReport() const {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗"
              << std::endl;
    std::cout << "║           Power Model Coverage Report                    ║"
              << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝"
              << std::endl;

    std::cout << "\nComponent Coverage:\n" << std::endl;
    std::cout << std::setw(20) << "Component"
              << std::setw(25) << "Primary Model"
              << std::setw(20) << "Fallback"
              << std::endl;
    std::cout << std::string(65, '-') << std::endl;

    const PowerComponent components[] = {
        PowerComponent::CORE,
        PowerComponent::L1_CACHE,
        PowerComponent::L2_CACHE,
        PowerComponent::L3_CACHE,
        PowerComponent::MEMORY_CONTROLLER,
        PowerComponent::MEMORY,
        PowerComponent::NETWORK_ROUTER,
        PowerComponent::NETWORK_LINK,
        PowerComponent::PE
    };

    for (auto comp : components) {
        std::string primary = "None";
        std::string fallback = "None";

        if (hasSpecializedModel(comp)) {
            if (comp == PowerComponent::MEMORY && ramulator_model_) {
                primary = "Ramulator";
            } else if ((comp == PowerComponent::NETWORK_ROUTER ||
                       comp == PowerComponent::NETWORK_LINK) && garnet_model_) {
                primary = "GARNET";
            } else if (custom_models_.find(comp) != custom_models_.end()) {
                primary = custom_model_names_.at(comp);
            }
            fallback = mcpat_fallback_enabled_ ? "McPAT" : "Analytical";
        } else {
            if (mcpat_fallback_enabled_ && mcpat_wrapper_) {
                primary = "McPAT";
                fallback = analytical_fallback_enabled_ ? "Analytical" : "None";
            } else if (analytical_fallback_enabled_) {
                primary = "Analytical";
                fallback = "None";
            }
        }

        std::cout << std::setw(20) << componentName(comp)
                  << std::setw(25) << primary
                  << std::setw(20) << fallback
                  << std::endl;
    }

    std::cout << std::string(65, '-') << std::endl;

    // Summary
    std::cout << "\nSummary:" << std::endl;
    std::cout << "  Specialized models: " << custom_models_.size();
    if (ramulator_model_) std::cout << " + Ramulator";
    if (garnet_model_) std::cout << " + GARNET";
    std::cout << std::endl;

    std::cout << "  McPAT fallback: "
              << (mcpat_fallback_enabled_ ? "✓ Enabled" : "✗ Disabled")
              << std::endl;
    std::cout << "  Analytical fallback: "
              << (analytical_fallback_enabled_ ? "✓ Enabled" : "✗ Disabled")
              << std::endl;

    std::cout << std::string(65, '=') << "\n" << std::endl;
}

void PowerModelManager::resetStats() {
    cached_power_.clear();
    activity_stats_.clear();
    usage_stats_.clear();

    if (mcpat_wrapper_) {
        // Reset McPAT statistics
        mcpat_wrapper_->computePower();  // Will reset internal state
    }
}

//=============================================================================
// Helper Functions
//=============================================================================

std::string PowerModelManager::componentName(PowerComponent component) const {
    switch (component) {
        case PowerComponent::CORE: return "Core";
        case PowerComponent::L1_CACHE: return "L1 Cache";
        case PowerComponent::L2_CACHE: return "L2 Cache";
        case PowerComponent::L3_CACHE: return "L3 Cache";
        case PowerComponent::MEMORY_CONTROLLER: return "Memory Controller";
        case PowerComponent::MEMORY: return "Memory (DRAM)";
        case PowerComponent::NETWORK_ROUTER: return "Network Router";
        case PowerComponent::NETWORK_LINK: return "Network Link";
        case PowerComponent::PE: return "Processing Element";
        default: return "Unknown";
    }
}

} // namespace pimid
