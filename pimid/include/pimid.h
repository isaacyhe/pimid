#ifndef PIMID_H
#define PIMID_H

/**
 * PIMID: A Full-System Simulator with Intricacy and Diversity for PIM
 *
 * Main header file providing the complete PIMID simulation framework
 */

#include "common/types.h"
#include "common/simulation_engine.h"
#include "common/config_parser.h"
#include "host_engine/host_engine.h"
#include "device_engine/device_engine.h"
#include "memory_models/memory_model.h"
#include "network/network_model.h"
#include "power/power_model.h"
#include "address_translation/pe_placement.h"
#include "scheduler/scheduler.h"

#include <memory>
#include <string>

namespace pimid {

/**
 * Main PIMID simulator class
 * Orchestrates host-device co-simulation with full PIM system modeling
 */
class PIMIDSimulator {
public:
    PIMIDSimulator();
    ~PIMIDSimulator();

    // Configuration
    bool loadConfiguration(const std::string& config_file);
    void setHostConfig(const std::string& config_path);
    void setDeviceConfig(const std::string& config_path);
    void setMemoryConfig(const std::string& config_path);
    void setNetworkConfig(const std::string& config_path);
    void setPowerConfig(const std::string& config_path);

    // Initialization
    bool initialize();
    void initializeHost();
    void initializeDevice();
    void initializeMemoryModels();
    void initializeNetworkModel();
    void initializePowerModel();

    // Simulation control
    void run(Cycle num_cycles);
    void runUntilCompletion();
    void pause();
    void resume();
    void stop();

    // Workload management
    void loadWorkload(const std::string& binary_path);
    void setWorkloadArgs(int argc, char** argv);

    // Statistics and reporting
    PIMIDStats getStats() const;
    void printStats() const;
    void exportStats(const std::string& output_file) const;
    void resetStats();

    // Status queries
    bool isInitialized() const { return initialized_; }
    bool isRunning() const { return running_; }
    Cycle getCurrentCycle() const;

    // Advanced features
    void enablePowerModeling(bool enable);
    void enableNetworkModeling(bool enable);
    void setSchedulingPolicy(SchedulingPolicy policy);

    // Debugging
    void enableDebugMode(bool enable);
    void setLogLevel(int level);
    void dumpState(const std::string& output_file);

private:
    // Configuration
    ConfigParser config_parser_;
    PIMIDConfig config_;
    bool initialized_;
    bool running_;

    // Simulation engines
    std::unique_ptr<HostEngine> host_engine_;
    std::unique_ptr<DeviceEngine> device_engine_;

    // Modeling infrastructure
    std::shared_ptr<MemoryModel> memory_model_;
    std::shared_ptr<NetworkModel> network_model_;
    std::shared_ptr<PowerModel> power_model_;

    // Statistics aggregation
    PIMIDStats aggregated_stats_;

    // Debug and logging
    bool debug_mode_;
    int log_level_;

    // Internal methods
    void synchronizeEngines();
    void collectStats();
    void validateConfiguration();
};

/**
 * Convenience functions for common use cases
 */

// Run a simple PIM simulation with default configuration
PIMIDStats runSimulation(const std::string& config_file,
                         const std::string& workload,
                         int argc, char** argv);

// Run with custom memory technology
PIMIDStats runWithMemoryTech(const std::string& config_file,
                             const std::string& workload,
                             MemoryTechnology tech);

// Run with custom PE placement
PIMIDStats runWithPEPlacement(const std::string& config_file,
                              const std::string& workload,
                              PEPlacementLevel level);

} // namespace pimid

#endif // PIMID_H
