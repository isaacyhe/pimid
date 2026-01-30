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
        bool is_main_memory;         // Is this main memory?
        uint32_t access_mode;        // 0=normal, 1=sequential, 2=fast

        // Output width
        uint32_t output_width_bits;  // Output width in bits

        // Optional tag configuration
        bool specific_tag;           // Use specific tag width
        uint32_t tag_width_bits;     // Tag width if specific_tag is true

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
            , associativity(8)
            , banks(8)
            , read_write_ports(1)
            , read_ports(0)
            , write_ports(0)
            , single_ended_read_ports(0)
            , tech_node_nm(22)
            , temperature(350)  // 350K = ~77C
            , is_cache(true)
            , is_main_memory(false)
            , access_mode(0)
            , output_width_bits(512)
            , specific_tag(false)
            , tag_width_bits(0)
            , obj_func_delay(0)
            , obj_func_dynamic_power(0)
            , obj_func_leakage_power(0)
            , obj_func_cycle_time(0)
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
