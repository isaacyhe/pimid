#ifndef PIMID_SRAM_MODEL_H
#define PIMID_SRAM_MODEL_H

#include "memory/memory_model.h"
#include "memory/cacti_wrapper.h"
#include "memory/sram_architecture.h"
#include <queue>
#include <memory>

namespace pimid {

/**
 * SRAM memory model using CACTI
 * Provides timing, area, and power modeling for SRAM caches/scratchpads
 */
class SRAMModel : public MemoryModel {
public:
    explicit SRAMModel(const std::string& config_path);
    ~SRAMModel() override = default;

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

    // SRAM-specific queries
    double getArea() const { return area_mm2_; }

    // Inner-bank timing queries (NEW!)
    double getSubarrayReadLatency() const;
    double getBankReadLatency() const;
    double getChipReadLatency() const;
    double getInnerBankDatapathLatency() const;  // Total inner-bank datapath

    // PIM support queries
    bool supportsBankPIM() const;
    bool supportsSubarrayPIM() const;

private:
    // SRAM-specific configuration
    struct SRAMConfig {
        uint64_t capacity;       // in bytes
        uint32_t line_size;      // cache line size
        uint32_t associativity;  // for cache-like SRAM
        uint32_t banks;
        uint32_t read_write_ports;
        uint32_t read_ports;
        uint32_t write_ports;
        uint32_t tech_node_nm;   // Technology node
        uint32_t temperature_k = 350;  // 1.11.52 (D055): run temperature, K
        Cycle access_time;
    };

    SRAMConfig sram_config_;

    // CACTI wrapper instance
    std::unique_ptr<CACTIWrapper> cacti_wrapper_;

    // SRAM architecture specifications (NEW!)
    std::unique_ptr<memory::SRAMArchitecture> sram_arch_;

    // Request queue
    std::queue<MemoryRequest> pending_requests_;

    // Statistics
    uint64_t total_reads_;
    uint64_t total_writes_;
    uint64_t total_accesses_;

    // Helper functions
    void useFallbackValues();

    // Energy and area from CACTI
    double read_energy_;
    double write_energy_;
    double leakage_power_;
    double area_mm2_;

    // Current state
    Cycle current_cycle_;
    uint64_t capacity_;
    uint64_t bandwidth_;

    //=== 1.11.24: the memory plugin contract (see MemoryModel) ===============
    double getTierLatencyNs(Tier tier, Op op) const override;
    bool hasTier(Tier tier) const override;
    std::string tierLatencySource(Tier tier, Op op) const override;
    void setArrayCapacityBytes(uint64_t bytes) override;
    void setAccessWidthBits(uint32_t bits) override;
    void setTechNodeNm(int nm) override;   // 1.11.51 (L70)
    void setTemperatureK(int k) override;  // 1.11.52 (D055)
    uint32_t access_width_bits_ = 0;   // 0 = model default
};

} // namespace pimid

#endif // PIMID_SRAM_MODEL_H
