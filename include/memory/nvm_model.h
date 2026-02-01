#ifndef PIMID_NVM_MODEL_H
#define PIMID_NVM_MODEL_H

#include "memory/memory_model.h"
#include "memory/sttmram_architecture.h"
#include "memory/pcm_architecture.h"
#include "memory/reram_architecture.h"
#include <queue>
#include <map>
#include <unordered_map>
#include <memory>

// Forward declaration
namespace pimid {
class NVSimWrapper;
}

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

    // Inner-bank timing queries (NEW!)
    double getSubarrayReadLatency() const;
    double getBankReadLatency() const;
    double getChipReadLatency() const;
    double getSubarrayWriteLatency() const;
    double getBankWriteLatency() const;
    double getChipWriteLatency() const;
    double getInnerBankReadLatency() const;
    double getInnerBankWriteLatency() const;

    // PIM support queries
    bool supportsBankPIM() const;
    bool supportsSubarrayPIM() const;

    // Get architecture specifications (returns appropriate type based on cell_type)
    const memory::STTMRAMArchitecture* getSTTMRAMArchitecture() const { return sttmram_arch_.get(); }
    const memory::PCMArchitecture* getPCMArchitecture() const { return pcm_arch_.get(); }
    const memory::ReRAMArchitecture* getReRAMArchitecture() const { return reram_arch_.get(); }
    std::string getCellType() const { return nvm_config_.cell_type; }

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

    // NVSim wrapper for accurate modeling (when HAVE_NVSIM defined)
    std::unique_ptr<NVSimWrapper> nvsim_wrapper_;

    // Architecture specifications (only one is populated based on cell_type)
    std::unique_ptr<memory::STTMRAMArchitecture> sttmram_arch_;
    std::unique_ptr<memory::PCMArchitecture> pcm_arch_;
    std::unique_ptr<memory::ReRAMArchitecture> reram_arch_;

    // Legacy placeholder (deprecated)
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
    void initializeNVSim();

    // Per-bank and per-page write counts for endurance tracking
    std::map<uint32_t, uint64_t> bank_write_counts_;
    std::unordered_map<uint64_t, uint64_t> page_write_counts_;
};

} // namespace pimid

#endif // PIMID_NVM_MODEL_H
