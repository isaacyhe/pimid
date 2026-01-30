#include "power_model.h"
#include "memory_model.h"
#include "network_model.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace pimid {

//=============================================================================
// PowerModel Base Implementation
//=============================================================================

PowerModel::PowerModel(const TechnologyParams& params)
    : tech_params_(params) {

    std::cout << "[PowerModel] Creating power model:" << std::endl;
    std::cout << "  Technology node: " << tech_params_.tech_node_nm << " nm" << std::endl;
    std::cout << "  Device type: " << tech_params_.device_type << std::endl;
    std::cout << "  Temperature: " << tech_params_.temperature_k << " K" << std::endl;
    std::cout << "  Frequency: " << tech_params_.frequency_ghz << " GHz" << std::endl;
    std::cout << "  Core count: " << tech_params_.core_count << std::endl;
}

//=============================================================================
// CompositePowerModel Implementation
//=============================================================================

CompositePowerModel::CompositePowerModel(const TechnologyParams& params)
    : PowerModel(params)
    , mcpat_model_(nullptr)
    , memory_model_(nullptr)
    , network_model_(nullptr) {
}

void CompositePowerModel::addMemoryModel(std::shared_ptr<class MemoryModel> mem_model) {
    memory_model_ = mem_model;
    std::cout << "[CompositePowerModel] Added memory model" << std::endl;
}

void CompositePowerModel::addNetworkModel(std::shared_ptr<class NetworkModel> net_model) {
    network_model_ = net_model;
    std::cout << "[CompositePowerModel] Added network model" << std::endl;
}

void CompositePowerModel::initialize() {
    std::cout << "[CompositePowerModel] Initializing composite power model..." << std::endl;

    // Initialize McPAT model
    mcpat_model_ = std::make_shared<McPATModel>(tech_params_);
    mcpat_model_->initialize();

    std::cout << "[CompositePowerModel] Initialization complete" << std::endl;
}

void CompositePowerModel::loadConfig(const std::string& config_path) {
    std::cout << "[CompositePowerModel] Loading configuration from: " << config_path << std::endl;

    if (mcpat_model_) {
        mcpat_model_->loadConfig(config_path);
    }
}

PowerMetrics CompositePowerModel::estimatePower(PowerComponent component,
                                                 const ActivityStats& stats) {
    PowerMetrics metrics;

    // Route to appropriate model
    switch (component) {
        case PowerComponent::CORE:
        case PowerComponent::L1_CACHE:
        case PowerComponent::L2_CACHE:
        case PowerComponent::L3_CACHE:
        case PowerComponent::MEMORY_CONTROLLER:
        case PowerComponent::PE:
            if (mcpat_model_) {
                metrics = mcpat_model_->estimatePower(component, stats);
            }
            break;

        case PowerComponent::MEMORY:
            if (memory_model_) {
                // Get power from actual memory model
                double total_energy = memory_model_->getTotalEnergy();  // in nJ
                double leakage = memory_model_->getLeakagePower();      // in W

                // Convert energy to power: P = E / t (assuming 1 GHz clock)
                // Total energy is cumulative, divide by simulation time
                double sim_time_s = stats.total_cycles / (tech_params_.frequency_ghz * 1e9);
                if (sim_time_s > 0) {
                    metrics.dynamic_power_w = (total_energy * 1e-9) / sim_time_s;  // nJ to J, then divide by time
                } else {
                    // Estimate based on access rates
                    double read_energy = memory_model_->getReadEnergy();   // nJ per access
                    double write_energy = memory_model_->getWriteEnergy(); // nJ per access
                    uint64_t total_accesses = stats.memory_reads + stats.memory_writes;
                    double access_rate = total_accesses * tech_params_.frequency_ghz * 1e9; // accesses/sec
                    metrics.dynamic_power_w = access_rate * ((read_energy + write_energy) / 2.0) * 1e-9;
                }
                metrics.leakage_power_w = leakage;
            } else {
                // Fallback: estimate based on technology node
                double scale = std::pow(22.0 / tech_params_.tech_node_nm, 2.0);
                metrics.dynamic_power_w = 5.0 * scale;
                metrics.leakage_power_w = 1.5 * scale;
            }
            break;

        case PowerComponent::NETWORK_ROUTER:
        case PowerComponent::NETWORK_LINK:
            if (network_model_) {
                // Get power from actual network model
                double total_energy = network_model_->getTotalEnergy();  // in J

                // Convert energy to power
                double sim_time_s = stats.total_cycles / (tech_params_.frequency_ghz * 1e9);
                if (sim_time_s > 0) {
                    double total_power = total_energy / sim_time_s;
                    if (component == PowerComponent::NETWORK_ROUTER) {
                        metrics.dynamic_power_w = network_model_->getRouterEnergy() / sim_time_s;
                    } else {
                        metrics.dynamic_power_w = network_model_->getLinkEnergy() / sim_time_s;
                    }
                } else {
                    metrics.dynamic_power_w = total_energy > 0 ? 2.0 : 0.0;
                }
                // Leakage estimate based on technology
                double scale = std::pow(22.0 / tech_params_.tech_node_nm, 1.5);
                metrics.leakage_power_w = 0.5 * scale;
            } else {
                // Fallback: estimate based on technology node
                double scale = std::pow(22.0 / tech_params_.tech_node_nm, 2.0);
                metrics.dynamic_power_w = 2.0 * scale;
                metrics.leakage_power_w = 0.5 * scale;
            }
            break;
    }

    metrics.total_power_w = metrics.dynamic_power_w + metrics.leakage_power_w;
    component_metrics_[component] = metrics;

    return metrics;
}

double CompositePowerModel::getDynamicPower(PowerComponent component) const {
    auto it = component_metrics_.find(component);
    if (it != component_metrics_.end()) {
        return it->second.dynamic_power_w;
    }
    return 0.0;
}

double CompositePowerModel::getLeakagePower(PowerComponent component) const {
    auto it = component_metrics_.find(component);
    if (it != component_metrics_.end()) {
        return it->second.leakage_power_w;
    }
    return 0.0;
}

double CompositePowerModel::getTotalPower() const {
    double total = 0.0;
    for (const auto& pair : component_metrics_) {
        total += pair.second.total_power_w;
    }
    return total;
}

double CompositePowerModel::getEnergy(PowerComponent component) const {
    auto it = component_metrics_.find(component);
    if (it != component_metrics_.end()) {
        return it->second.total_energy_j;
    }
    return 0.0;
}

double CompositePowerModel::getTotalEnergy() const {
    double total = 0.0;
    for (const auto& pair : component_metrics_) {
        total += pair.second.total_energy_j;
    }
    return total;
}

void CompositePowerModel::updateActivity(PowerComponent component,
                                          const ActivityStats& stats) {
    activity_stats_[component] = stats;

    if (mcpat_model_) {
        mcpat_model_->updateActivity(component, stats);
    }
}

void CompositePowerModel::printStats() const {
    std::cout << "\n=== Composite Power Model Statistics ===" << std::endl;

    if (mcpat_model_) {
        mcpat_model_->printStats();
    }

    std::cout << "\nTotal System Power: " << getTotalPower() << " W" << std::endl;
    std::cout << "Total System Energy: " << getTotalEnergy() << " J" << std::endl;
    std::cout << "========================================\n" << std::endl;
}

void CompositePowerModel::resetStats() {
    component_metrics_.clear();
    activity_stats_.clear();

    if (mcpat_model_) {
        mcpat_model_->resetStats();
    }
}

} // namespace pimid
