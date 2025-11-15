#ifndef PIMID_NVM_MODEL_H
#define PIMID_NVM_MODEL_H

#include "memory_models/memory_model.h"
#include <queue>

namespace pimid {

/**
 * Non-volatile memory model using NVSim
 * Supports STT-MRAM, PCM, ReRAM, etc.
 */
class NVMModel : public MemoryModel {
public:
    explicit NVMModel(const std::string& config_path);
    ~NVMModel() override = default;

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

    // NVM-specific queries
    double getArea() const { return area_mm2_; }
    uint64_t getEndurance() const { return endurance_; }

private:
    // NVM-specific configuration
    struct NVMConfig {
        std::string cell_type;   // STT-MRAM, PCM, ReRAM, etc.
        uint64_t capacity;
        uint32_t banks;
        uint32_t read_write_ports;
        uint32_t tech_node_nm;
        Cycle read_latency;
        Cycle write_latency;
        uint64_t endurance;      // Write endurance
        bool is_pim_enabled;     // Support for in-memory compute
    };

    NVMConfig nvm_config_;

    // NVSim interface (placeholder - will integrate actual NVSim)
    void* nvsim_instance_;

    // Request queue
    std::queue<MemoryRequest> pending_requests_;

    // Statistics
    uint64_t total_reads_;
    uint64_t total_writes_;
    uint64_t write_cycles_;  // Track endurance

    // Energy and area from NVSim
    double read_energy_;
    double write_energy_;
    double leakage_power_;
    double area_mm2_;

    // Current state
    Cycle current_cycle_;
    uint64_t capacity_;
    uint64_t bandwidth_;
    uint64_t endurance_;

    // Endurance tracking
    void updateEndurance(Address addr);
};

} // namespace pimid

#endif // PIMID_NVM_MODEL_H
