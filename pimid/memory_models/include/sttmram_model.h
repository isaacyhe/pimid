#ifndef PIMID_STTMRAM_MODEL_H
#define PIMID_STTMRAM_MODEL_H

#include "memory_model.h"
#include "nvsim_wrapper.h"
#include "memory/sttmram_architecture.h"
#include <queue>
#include <memory>

namespace pimid {

/**
 * STT-MRAM memory model using NVSim
 * Provides timing, area, and power modeling with inner-bank timing details
 */
class STTMRAMModel : public MemoryModel {
public:
    explicit STTMRAMModel(const std::string& config_path);
    ~STTMRAMModel() override = default;

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

    // STT-MRAM-specific queries
    double getArea() const { return area_mm2_; }
    uint64_t getEndurance() const { return endurance_; }

    // Inner-bank timing queries (NEW!)
    double getSubarrayReadLatency() const;
    double getBankReadLatency() const;
    double getChipReadLatency() const;
    double getSubarrayWriteLatency() const;
    double getBankWriteLatency() const;
    double getChipWriteLatency() const;
    double getInnerBankReadLatency() const;   // Total read path
    double getInnerBankWriteLatency() const;  // Total write path (MTJ switching!)

    // PIM support queries
    bool supportsBankPIM() const;
    bool supportsSubarrayPIM() const;

private:
    // STT-MRAM-specific configuration
    struct STTMRAMConfig {
        uint64_t capacity;
        uint32_t banks;
        uint32_t read_write_ports;
        uint32_t tech_node_nm;
        Cycle read_latency;
        Cycle write_latency;
        uint64_t endurance;
        bool is_pim_enabled;
    };

    STTMRAMConfig mram_config_;

    // NVSim wrapper instance
    std::unique_ptr<NVSimWrapper> nvsim_wrapper_;

    // STT-MRAM architecture specifications (NEW!)
    std::unique_ptr<memory::STTMRAMArchitecture> mram_arch_;

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

#endif // PIMID_STTMRAM_MODEL_H
