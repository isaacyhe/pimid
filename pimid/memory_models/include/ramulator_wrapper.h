#ifndef PIMID_RAMULATOR_WRAPPER_H
#define PIMID_RAMULATOR_WRAPPER_H

#include "common/types.h"
#include <string>
#include <memory>
#include <functional>

// Forward declarations for Ramulator types
namespace Ramulator {
    class IMemorySystem;
    struct Request;
}

namespace pimid {

/**
 * Wrapper class that adapts Ramulator2 to PIMID's memory model interface
 * This provides a clean separation between PIMID and Ramulator code
 */
class RamulatorWrapper {
public:
    explicit RamulatorWrapper(const std::string& config_path);
    ~RamulatorWrapper();

    // Initialization
    void initialize();
    void loadConfig(const std::string& config_path);

    // Request handling
    bool send(Address addr, MemoryRequestType type,
              std::function<void(Address)> callback = nullptr);
    bool canAccept() const;
    void tick();

    // Statistics and metrics
    uint64_t getTotalReads() const { return total_reads_; }
    uint64_t getTotalWrites() const { return total_writes_; }
    uint64_t getRowHits() const { return row_hits_; }
    uint64_t getRowMisses() const { return row_misses_; }
    uint64_t getRowConflicts() const { return row_conflicts_; }

    // Energy metrics (from Ramulator statistics)
    double getReadEnergy() const;
    double getWriteEnergy() const;
    double getActivationEnergy() const;
    double getPrechargeEnergy() const;
    double getRefreshEnergy() const;
    double getLeakagePower() const;
    double getTotalEnergy() const;

    // Configuration queries
    uint64_t getCapacity() const { return capacity_; }
    uint64_t getBandwidth() const { return bandwidth_; }
    Cycle getAverageLatency() const;

    // Cycle tracking
    Cycle getCurrentCycle() const { return current_cycle_; }

    void printStats() const;
    void resetStats();

private:
    // Ramulator memory system instance
    std::shared_ptr<Ramulator::IMemorySystem> ramulator_memory_system_;

    // Configuration
    std::string config_path_;
    std::string config_yaml_;  // YAML configuration content

    // Memory system parameters
    uint64_t capacity_;
    uint64_t bandwidth_;
    uint32_t channels_;
    uint32_t ranks_per_channel_;
    uint32_t banks_per_rank_;

    // Statistics tracking
    uint64_t total_reads_;
    uint64_t total_writes_;
    uint64_t row_hits_;
    uint64_t row_misses_;
    uint64_t row_conflicts_;

    // Energy tracking
    mutable double cached_read_energy_;
    mutable double cached_write_energy_;
    mutable double cached_leakage_power_;
    mutable Cycle last_energy_update_;

    // Cycle counter
    Cycle current_cycle_;

    // Pending request tracking
    struct PendingRequest {
        Address addr;
        MemoryRequestType type;
        Cycle issue_cycle;
        std::function<void(Address)> callback;
    };
    std::vector<PendingRequest> pending_requests_;

    // Helper functions
    void parseConfiguration();
    void createRamulatorInstance();
    Ramulator::Request createRamulatorRequest(Address addr, MemoryRequestType type);
    void handleRequestCompletion(Ramulator::Request& req);
    void updateEnergyMetrics() const;
};

} // namespace pimid

#endif // PIMID_RAMULATOR_WRAPPER_H
