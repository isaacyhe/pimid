#ifndef PIMID_SIMULATION_ENGINE_H
#define PIMID_SIMULATION_ENGINE_H

#include "common/types.h"
#include <memory>
#include <functional>

namespace pimid {

// Forward declarations
class MemoryModel;
class NetworkModel;
class PowerModel;
class EventQueue;

/**
 * Base class for simulation engines (host and device)
 * Provides common interface for cycle-accurate simulation
 */
class SimulationEngine {
public:
    SimulationEngine(SimulationDomain domain, const PIMIDConfig& config);
    virtual ~SimulationEngine() = default;

    // Core simulation interface
    virtual void initialize() = 0;
    virtual void run(Cycle num_cycles) = 0;
    virtual void finalize() = 0;

    // Timing and synchronization
    virtual Cycle getCurrentCycle() const { return current_cycle_; }
    virtual void advanceCycle(Cycle delta = 1) { current_cycle_ += delta; }
    virtual void synchronize(Cycle target_cycle);

    // Memory interface
    virtual void issueMemoryRequest(const MemoryRequest& req);
    virtual void handleMemoryResponse(const MemoryRequest& req, Cycle latency);

    // Statistics collection
    virtual PIMIDStats getStats() const { return stats_; }
    virtual void resetStats();
    virtual void printStats() const;

    // Configuration
    SimulationDomain getDomain() const { return domain_; }
    const PIMIDConfig& getConfig() const { return config_; }

protected:
    SimulationDomain domain_;
    PIMIDConfig config_;
    Cycle current_cycle_;
    PIMIDStats stats_;

    // Model interfaces
    std::shared_ptr<MemoryModel> memory_model_;
    std::shared_ptr<NetworkModel> network_model_;
    std::shared_ptr<PowerModel> power_model_;
    std::shared_ptr<EventQueue> event_queue_;

    // Callbacks for cross-domain communication
    std::function<void(const MemoryRequest&)> offload_callback_;
    std::function<void(Cycle)> sync_callback_;
};

} // namespace pimid

#endif // PIMID_SIMULATION_ENGINE_H
