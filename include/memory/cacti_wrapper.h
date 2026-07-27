#ifndef PIMID_CACTI_WRAPPER_H
#define PIMID_CACTI_WRAPPER_H

#include "common/types.h"
#include <string>
#include <memory>

// Forward declaration for CACTI types
class uca_org_t;
class InputParameter;

namespace pimid {

/**
 * Wrapper class that adapts CACTI to PIMID's memory model interface
 * Provides area, timing, and power modeling for SRAM caches/scratchpads
 */
class CACTIWrapper {
public:
    // Cell technology: 0=ITRS-HP SRAM, 1=ITRS-LSTP SRAM, 2=ITRS-LOP SRAM,
    //                  3=LP-DRAM, 4=Commercial DRAM
    enum CellType { SRAM_HP = 0, SRAM_LSTP = 1, SRAM_LOP = 2, LP_DRAM = 3, COMM_DRAM = 4 };

    struct SRAMConfig {
        // Capacity and organization
        uint64_t capacity_bytes;     // Total capacity in bytes
        uint32_t line_size;          // Cache line size in bytes
        uint32_t associativity;      // Set associativity
        uint32_t banks;              // Number of banks

        // Port configuration
        uint32_t read_write_ports;   // Number of RW ports
        uint32_t read_ports;         // Number of read-only ports
        uint32_t write_ports;        // Number of write-only ports
        uint32_t single_ended_read_ports; // Number of single-ended read ports

        // Technology
        double tech_node_nm;         // Technology node in nm (e.g., 22, 14, 7)
        uint32_t temperature;        // Operating temperature in Celsius

        // Access mode and type
        bool is_cache;               // true for cache, false for scratchpad
        bool is_main_memory;         // Is this main memory (DRAM)?
        uint32_t access_mode;        // 0=normal, 1=sequential, 2=fast

        // Cell technology (SRAM_HP default; set LP_DRAM/COMM_DRAM for DRAM mode)
        CellType cell_type;

        // DRAM-specific parameters (used when cell_type is LP_DRAM or COMM_DRAM)
        uint32_t page_sz_bits;       // Page size in bits (e.g., 8192 = 1KB page)
        uint32_t burst_len;          // Burst length (e.g., 8 for DDR4)
        uint32_t int_prefetch_w;     // Internal prefetch width

        // Output width
        uint32_t output_width_bits;  // Output width in bits

        // Optional tag configuration
        bool specific_tag;           // Use specific tag width
        uint32_t tag_width_bits;     // Tag width if specific_tag is true

        // Suppress the init banner (for latency-only queries whose energy
        // numbers are never consumed and would only mislead in logs)
        bool quiet = false;

        // Optimization objectives (0=weight, 1=deviate, 2=ED, 3=ED2)
        int obj_func_delay;
        int obj_func_dynamic_power;
        int obj_func_leakage_power;
        int obj_func_cycle_time;
        int obj_func_area;

        // Default constructor with sensible defaults
        SRAMConfig()
            : capacity_bytes(256 * 1024)  // 256 KB
            , line_size(64)
            , associativity(1)
            , banks(4)
            , read_write_ports(1)
            , read_ports(0)
            , write_ports(0)
            , single_ended_read_ports(0)
            , tech_node_nm(22)
            , temperature(350)  // 350K = ~77C
            , is_cache(false)
            , is_main_memory(false)
            , access_mode(0)
            , cell_type(SRAM_HP)
            , page_sz_bits(8192)
            , burst_len(8)
            , int_prefetch_w(8)
            , output_width_bits(512)
            , specific_tag(false)
            , tag_width_bits(0)
            , obj_func_delay(0)
            , obj_func_dynamic_power(0)
            , obj_func_leakage_power(0)
            , obj_func_cycle_time(100)  // Optimize for cycle time by default
            , obj_func_area(0)
        {}
    };

    explicit CACTIWrapper(const SRAMConfig& config);
    ~CACTIWrapper();

    // Initialization and configuration
    void initialize();
    void reconfigure(const SRAMConfig& config);

    // Query CACTI results
    double getAccessTime() const;      // Access time in seconds
    double getCycleTime() const;       // Cycle time in seconds
    double getArea() const;            // Area in mm^2
    double getDynamicReadEnergy() const;   // Read energy in nJ
    double getDynamicWriteEnergy() const;  // Write energy in nJ
    double getLeakagePower() const;    // Leakage power in mW

    // Detailed power breakdown
    double getReadDynamicPower() const;    // Dynamic power during read (mW)
    double getWriteDynamicPower() const;   // Dynamic power during write (mW)
    double getGateLeakagePower() const;    // Gate leakage power (mW)

    // Physical characteristics
    double getCacheHeight() const;     // Cache height in mm
    double getCacheWidth() const;      // Cache width in mm
    double getAreaEfficiency() const;  // Area efficiency (0-1)

    // Timing characteristics
    Cycle getAccessLatencyCycles(double freq_hz) const;
    Cycle getCycleTimeCycles(double freq_hz) const;

    //=========================================================================
    // CACTI 7.0 Subarray-Level Characteristics (NEW)
    //=========================================================================

    // Subarray timing breakdown (in seconds)
    double getDecoderDelay() const;           // Row decoder delay
    double getWordlineDelay() const;          // Wordline activation delay
    double getBitlineDelay() const;           // Bitline sensing delay
    double getSenseAmpDelay() const;          // Sense amplifier delay
    double getSubarrayOutputDelay() const;    // Subarray output driver delay
    double getHtreeDelay() const;             // H-tree interconnect delay

    // Subarray organization
    uint32_t getSubarrayRows() const;         // Number of rows per subarray
    uint32_t getSubarrayCols() const;         // Number of columns per subarray
    uint32_t getSubarraysPerMat() const;      // Subarrays per mat
    uint32_t getMatsPerBank() const;          // Mats per bank

    // Subarray electrical parameters
    double getWordlineCapacitance() const;    // Wordline capacitance (F)
    double getWordlineResistance() const;     // Wordline resistance (Ohm)
    double getBitlineCapacitance() const;     // Bitline capacitance (F)

    // Subarray energy breakdown (in nJ)
    double getDecoderEnergy() const;
    double getWordlineEnergy() const;
    double getBitlineEnergy() const;
    double getSenseAmpEnergy() const;

    // Subarray leakage breakdown (in mW)
    double getArrayLeakage() const;
    double getWordlineLeakage() const;
    double getColumnLeakage() const;

    // Subarray area
    double getSubarrayArea() const;           // Per-subarray area (mm^2)
    double getCellArea() const;               // Per-cell area (um^2)

    //=========================================================================

    // Validation
    bool isValid() const;
    std::string getErrorMessage() const;

    // Configuration access
    const SRAMConfig& getConfig() const { return config_; }

    // Print detailed results
    void printDetailedResults() const;

private:
    // Configuration
    SRAMConfig config_;

    // CACTI result (using raw pointer with manual management)
    uca_org_t* cacti_result_;
    InputParameter* cacti_input_;

    // Cached values
    bool initialized_;
    bool valid_;
    std::string error_message_;

    // Helper functions
    void runCACTI();
    void validateConfiguration();
    InputParameter* createCACTIInput(const SRAMConfig& config);
};

} // namespace pimid

#endif // PIMID_CACTI_WRAPPER_H
