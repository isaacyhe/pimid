#ifndef PIMID_PCM_MODEL_H
#define PIMID_PCM_MODEL_H

#include "memory/memory_model.h"
#include "memory/nvsim_wrapper.h"
#include "memory/pcm_architecture.h"
#include <queue>
#include <memory>
#include <map>
#include <unordered_map>

namespace pimid {

/**
 * PCM (Phase-Change Memory) model using NVSim
 * Provides timing, area, and power modeling with inner-bank timing details
 *
 * CRITICAL: PCM has VERY slow writes (50-150ns SET, 10-50ns RESET)
 * Only suitable for read-heavy PIM workloads!
 */
class PCMModel : public MemoryModel {
public:
    explicit PCMModel(const std::string& config_path);
    ~PCMModel() override = default;

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

    // PCM-specific queries
    double getArea() const { return area_mm2_; }
    uint64_t getEndurance() const { return endurance_; }

    // Inner-bank timing queries (NEW!)
    double getSubarrayReadLatency() const;
    double getBankReadLatency() const;
    double getChipReadLatency() const;
    double getSubarraySetWriteLatency() const;   // Crystallization (SLOW!)
    double getBankSetWriteLatency() const;
    double getChipSetWriteLatency() const;
    double getSubarrayResetWriteLatency() const; // Amorphization (faster)
    double getBankResetWriteLatency() const;
    double getChipResetWriteLatency() const;
    double getInnerBankReadLatency() const;

    // PIM support queries
    bool supportsBankPIM() const;
    bool supportsSubarrayPIM() const;
    bool isReadOnlyPIM() const { return true; }  // Only read-heavy workloads!

private:
    // PCM-specific configuration
    struct PCMConfig {
        uint64_t capacity;
        uint32_t banks;
        uint32_t read_write_ports;
        uint32_t tech_node_nm;
        Cycle read_latency;
        Cycle set_write_latency;    // SET (crystallization)
        Cycle reset_write_latency;  // RESET (amorphization)
        uint64_t endurance;
        bool is_pim_enabled;
    };

    PCMConfig pcm_config_;

    // NVSim wrapper instance
    std::unique_ptr<NVSimWrapper> nvsim_wrapper_;

    // PCM architecture specifications (NEW!)
    std::unique_ptr<memory::PCMArchitecture> pcm_arch_;

    // Request queue
    std::queue<MemoryRequest> pending_requests_;

    // Statistics
    uint64_t total_reads_;
    uint64_t total_set_writes_;    // SET operations
    uint64_t total_reset_writes_;  // RESET operations
    uint64_t write_cycles_;

    // Energy and area from NVSim
    double read_energy_;
    double write_energy_;  // Average of SET/RESET
    double leakage_power_;
    double area_mm2_;

    // Current state
    Cycle current_cycle_;
    uint64_t capacity_;
    uint64_t bandwidth_;
    uint64_t endurance_;

    // NVSim initialization (NEW!)
    void initializeNVSim();

    // Endurance tracking with wear-leveling support
    void updateEndurance(Address addr);
    void reportWearImbalance() const;

    // Per-bank, per-page, and per-cell write counts for wear tracking
    // PCM has limited endurance (~10^8), so tracking is critical!
    std::map<uint32_t, uint64_t> bank_write_counts_;
    std::unordered_map<uint64_t, uint64_t> page_write_counts_;
    std::unordered_map<uint64_t, uint64_t> cell_write_counts_;

    //=== 1.11.24: the memory plugin contract (see MemoryModel) ===============
    double getTierLatencyNs(Tier tier, Op op) const override;
    bool hasTier(Tier tier) const override;
    std::string tierLatencySource(Tier tier, Op op) const override;
    void setArrayCapacityBytes(uint64_t bytes) override;
    void setAccessWidthBits(uint32_t bits) override;
    void setTechNodeNm(int nm) override;   // 1.11.51 (L70)
    uint32_t access_width_bits_ = 0;   // 0 = model default
};

} // namespace pimid

#endif // PIMID_PCM_MODEL_H
