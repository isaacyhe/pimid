#ifndef PIMID_NVSIM_WRAPPER_H
#define PIMID_NVSIM_WRAPPER_H

#include "common/types.h"
#include <string>
#include <memory>

// Forward declarations for NVSim types
class InputParameter;
class MemCell;
class Result;

namespace pimid {

/**
 * Wrapper class that adapts NVSim to PIMID's memory model interface
 * Provides area, timing, and power modeling for Non-Volatile Memory
 * Supports: STT-RAM, PCM, ReRAM, FBDRAM, NAND Flash
 */
class NVSimWrapper {
public:
    enum class NVMType {
        STTRAM,        // Spin-Transfer Torque RAM
        PCRAM,         // Phase-Change RAM
        RERAM,         // Resistive RAM
        FBDRAM,        // Fine-grained DRAM
        SLCNAND,       // Single-Level Cell NAND Flash
        MLCNAND        // Multi-Level Cell NAND Flash
    };

    struct NVMConfig {
        // Basic configuration
        uint64_t capacity_bytes;     // Total capacity in bytes
        uint32_t word_width_bits;    // Word width in bits
        NVMType nvm_type;            // Type of NVM

        // Technology parameters
        int process_node_nm;         // Technology node in nm
        int temperature_k;           // Operating temperature in Kelvin

        // Optimization target
        bool optimize_read_latency;
        bool optimize_write_latency;
        bool optimize_read_energy;
        bool optimize_write_energy;
        bool optimize_leakage;
        bool optimize_area;

        // Cell parameters file (optional, uses defaults if empty)
        std::string cell_file;

        // For Flash memory
        uint64_t page_size_bits;     // Page size (for DRAM/Flash)
        uint64_t block_size_bits;    // Block size (for Flash)

        // Cache-specific (if used as cache)
        int associativity;           // 0 = fully associative, >0 = N-way
        bool is_cache;               // true if used as cache

        // Default constructor with sensible defaults for STT-RAM
        NVMConfig()
            : capacity_bytes(8 * 1024 * 1024)  // 8 MB default
            , word_width_bits(64)
            , nvm_type(NVMType::STTRAM)
            , process_node_nm(22)
            , temperature_k(350)  // ~77°C
            , optimize_read_latency(false)
            , optimize_write_latency(false)
            , optimize_read_energy(true)
            , optimize_write_energy(true)
            , optimize_leakage(true)
            , optimize_area(false)
            , cell_file("")
            , page_size_bits(0)
            , block_size_bits(0)
            , associativity(0)
            , is_cache(false)
        {}
    };

    explicit NVSimWrapper(const NVMConfig& config);
    ~NVSimWrapper();

    // Initialization and configuration
    void initialize();
    void reconfigure(const NVMConfig& config);

    // Query NVSim results - Timing
    double getReadLatency() const;      // Read latency in seconds
    double getWriteLatency() const;     // Write latency in seconds

    // Query NVSim results - Energy
    double getReadDynamicEnergy() const;    // Read energy in nJ
    double getWriteDynamicEnergy() const;   // Write energy in nJ
    double getLeakagePower() const;         // Leakage power in mW
    double getReadEDP() const;              // Read Energy-Delay Product
    double getWriteEDP() const;             // Write Energy-Delay Product

    // Query NVSim results - Area
    double getArea() const;                 // Total area in mm^2
    double getHeight() const;               // Height in mm
    double getWidth() const;                // Width in mm

    // Cell-level metrics
    double getCellReadLatency() const;      // Cell read latency in seconds
    double getCellWriteLatency() const;     // Cell write latency in seconds
    double getCellArea() const;             // Cell area in um^2

    // Memory organization
    uint32_t getNumRows() const;
    uint32_t getNumColumns() const;
    uint32_t getNumBanks() const;

    // Bandwidth and throughput
    double getReadBandwidth() const;        // Read bandwidth in GB/s
    double getWriteBandwidth() const;       // Write bandwidth in GB/s

    // Validation
    bool isValid() const;
    std::string getErrorMessage() const;

    // Configuration access
    const NVMConfig& getConfig() const { return config_; }

    // Print detailed results
    void printDetailedResults() const;

private:
    // Configuration
    NVMConfig config_;

    // NVSim objects (using raw pointers with manual management)
    InputParameter* nvsim_input_;
    MemCell* nvsim_cell_;
    Result* nvsim_result_;

    // Cached values
    bool initialized_;
    bool valid_;
    std::string error_message_;

    // Helper functions
    void runNVSim();
    void validateConfiguration();
    void createNVSimInput();
    void loadCellParameters();
    std::string getCellFileName() const;
};

} // namespace pimid

#endif // PIMID_NVSIM_WRAPPER_H
