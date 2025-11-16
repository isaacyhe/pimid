#ifndef PIMID_SRAM_MODEL_H
#define PIMID_SRAM_MODEL_H

#include "memory_model.h"
#include <queue>

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
        Cycle access_time;
    };

    SRAMConfig sram_config_;

    // CACTI interface (placeholder - will integrate actual CACTI)
    void* cacti_instance_;

    // Request queue
    std::queue<MemoryRequest> pending_requests_;

    // Statistics
    uint64_t total_reads_;
    uint64_t total_writes_;
    uint64_t total_accesses_;

    // Energy and area from CACTI
    double read_energy_;
    double write_energy_;
    double leakage_power_;
    double area_mm2_;

    // Current state
    Cycle current_cycle_;
    uint64_t capacity_;
    uint64_t bandwidth_;
};

} // namespace pimid

#endif // PIMID_SRAM_MODEL_H
