#ifndef PIMID_DRAM_MODEL_H
#define PIMID_DRAM_MODEL_H

#include "memory/memory_model.h"
#include "memory/dram_architecture_v2.h"
#include <queue>
#include <map>
#include <memory>

namespace pimid {

// Forward declaration
class RamulatorWrapper;

/**
 * DRAM memory model using Ramulator
 * Provides cycle-accurate DRAM timing and power modeling
 */
class DRAMModel : public MemoryModel {
public:
    explicit DRAMModel(const std::string& config_path);
    ~DRAMModel() override;  // Defined in .cpp for unique_ptr<RamulatorWrapper>

    // MemoryModel interface implementation
    void initialize() override;
    void loadConfig(const std::string& config_path) override;

    Cycle access(const MemoryRequest& req) override;
    bool canAccept(const MemoryRequest& req) override;
    void tick() override;

    // Energy modeling
    double getReadEnergy() const override { return read_energy_; }
    double getWriteEnergy() const override { return write_energy_; }
    double getLeakagePower() const override { return leakage_power_; }
    double getTotalEnergy() const override;

    // Configuration queries
    uint64_t getCapacity() const override { return capacity_; }
    uint64_t getBandwidth() const override { return bandwidth_; }
    Cycle getLatency(MemoryRequestType type) const override;

    // Statistics
    void printStats() const override;
    void resetStats() override;

    // Inner-bank timing queries (NEW!)
    double getSubarrayAccessLatency() const;
    double getBankAccessLatency() const;
    double getChipAccessLatency() const;
    double getInnerBankDatapathDelay() const;

    // PIM support queries
    bool supportsBankPIM() const;
    bool supportsSubarrayPIM() const;

    // Get architecture specification
    const memory::DRAMArchitectureV2* getDRAMArchitecture() const { return dram_arch_.get(); }

private:
    // DRAM-specific configuration
    struct DRAMConfig {
        std::string standard;  // DDR3, DDR4, GDDR5, HBM, etc.
        std::string org;       // Organization (e.g., "4Gb_x8")
        uint32_t channels;
        uint32_t ranks_per_channel;
        uint32_t banks_per_rank;
        uint64_t capacity;
        uint64_t bandwidth;
        Cycle tCL;   // CAS latency
        Cycle tRCD;  // RAS to CAS delay
        Cycle tRP;   // Row precharge time
        Cycle tRAS;  // Row active time
    };

    DRAMConfig dram_config_;

    // Ramulator interface
    std::unique_ptr<RamulatorWrapper> ramulator_instance_;

    // DRAM architecture specifications (NEW!)
    std::unique_ptr<memory::DRAMArchitectureV2> dram_arch_;

    // Request queue
    std::queue<MemoryRequest> pending_requests_;
    std::map<Address, Cycle> row_buffer_state_;

    // Statistics
    uint64_t total_reads_;
    uint64_t total_writes_;
    uint64_t row_hits_;
    uint64_t row_misses_;
    uint64_t row_conflicts_;

    // Energy tracking
    double read_energy_;
    double write_energy_;
    double leakage_power_;
    double activation_energy_;
    double precharge_energy_;

    // Current state
    Cycle current_cycle_;
    uint64_t capacity_;
    uint64_t bandwidth_;

    // Helper functions
    Cycle calculateLatency(const MemoryRequest& req);
    void updateRowBuffer(Address addr);
    bool isRowHit(Address addr) const;

    //=== 1.11.24: the memory plugin contract (see MemoryModel) ===============
    double getTierLatencyNs(Tier tier, Op op) const override;
    bool hasTier(Tier tier) const override;
    std::string tierLatencySource(Tier tier, Op op) const override;
};

} // namespace pimid

#endif // PIMID_DRAM_MODEL_H
