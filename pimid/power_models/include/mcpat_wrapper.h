#ifndef PIMID_MCPAT_WRAPPER_H
#define PIMID_MCPAT_WRAPPER_H

#include "common/types.h"
#include <string>
#include <memory>
#include <map>

// Forward declarations for McPAT types
class ParseXML;
class Processor;

namespace pimid {

/**
 * Wrapper class that adapts McPAT to PIMID's power model interface
 * Provides integrated power, area, and timing modeling for:
 * - CPU cores
 * - Caches (L1, L2, L3)
 * - NoCs (Network-on-Chip)
 * - Memory controllers
 */
class McPATWrapper {
public:
    enum class ComponentType {
        CORE,              // CPU core
        L1_CACHE,          // L1 cache
        L2_CACHE,          // L2 cache
        L3_CACHE,          // L3 cache (shared)
        NOC,               // Network-on-Chip
        MEMORY_CONTROLLER, // Memory controller
        FULL_SYSTEM        // Entire processor system
    };

    struct PowerMetrics {
        // Dynamic power (W)
        double subthreshold_leakage;
        double gate_leakage;
        double runtime_dynamic;
        double total_leakage;
        double total_dynamic;
        double total_power;

        // Energy (nJ)
        double total_energy;

        PowerMetrics()
            : subthreshold_leakage(0.0)
            , gate_leakage(0.0)
            , runtime_dynamic(0.0)
            , total_leakage(0.0)
            , total_dynamic(0.0)
            , total_power(0.0)
            , total_energy(0.0)
        {}
    };

    struct SystemConfig {
        // Core parameters
        int num_cores;
        double core_clock_mhz;
        int pipeline_depth;
        int issue_width;

        // Cache parameters
        uint64_t l1i_size_bytes;
        uint64_t l1d_size_bytes;
        uint64_t l2_size_bytes;
        uint64_t l3_size_bytes;

        // Memory parameters
        int num_memory_controllers;
        double mc_clock_mhz;

        // NoC parameters
        bool has_noc;
        int noc_topology;  // 0=mesh, 1=crossbar, 2=bus

        // Technology
        int tech_node_nm;
        int temperature_k;

        // XML configuration file (optional)
        std::string xml_file;

        // Default constructor with sensible defaults
        SystemConfig()
            : num_cores(4)
            , core_clock_mhz(2000.0)  // 2 GHz
            , pipeline_depth(14)
            , issue_width(4)
            , l1i_size_bytes(32 * 1024)  // 32 KB
            , l1d_size_bytes(32 * 1024)  // 32 KB
            , l2_size_bytes(256 * 1024)  // 256 KB
            , l3_size_bytes(8 * 1024 * 1024)  // 8 MB
            , num_memory_controllers(1)
            , mc_clock_mhz(800.0)  // 800 MHz
            , has_noc(true)
            , noc_topology(0)  // Mesh
            , tech_node_nm(22)
            , temperature_k(350)  // ~77°C
            , xml_file("")
        {}
    };

    explicit McPATWrapper(const SystemConfig& config);
    ~McPATWrapper();

    // Initialization and configuration
    void initialize();
    void reconfigure(const SystemConfig& config);

    // Set runtime statistics (needed for dynamic power calculation)
    void setTotalCycles(uint64_t cycles);
    void setBusyCycles(uint64_t cycles);
    void setTotalInstructions(uint64_t instructions);
    void setL1Accesses(uint64_t reads, uint64_t writes);
    void setL2Accesses(uint64_t reads, uint64_t writes);
    void setL3Accesses(uint64_t reads, uint64_t writes);

    // Run power analysis
    void computePower();

    // Query power results by component
    PowerMetrics getComponentPower(ComponentType component) const;
    PowerMetrics getSystemPower() const;

    // Specific component queries
    double getCorePower() const;           // Total power for all cores
    double getCachePower() const;          // Total cache hierarchy power
    double getMemoryControllerPower() const;
    double getNoCP

ower() const;

    // Area queries
    double getComponentArea(ComponentType component) const;  // mm^2
    double getTotalArea() const;  // mm^2

    // Peak power (useful for thermal design)
    double getPeakPower() const;  // W

    // Energy for a given time period
    double getEnergyForPeriod(double time_seconds) const;  // Joules

    // Validation
    bool isValid() const;
    std::string getErrorMessage() const;

    // Configuration access
    const SystemConfig& getConfig() const { return config_; }

    // Print detailed results
    void printDetailedResults() const;
    void printComponentBreakdown() const;

private:
    // Configuration
    SystemConfig config_;

    // McPAT objects
    ParseXML* mcpat_parser_;
    Processor* mcpat_processor_;

    // Cached power results
    std::map<ComponentType, PowerMetrics> component_power_;
    PowerMetrics system_power_;

    // Runtime statistics
    uint64_t total_cycles_;
    uint64_t busy_cycles_;
    uint64_t total_instructions_;
    uint64_t l1_reads_;
    uint64_t l1_writes_;
    uint64_t l2_reads_;
    uint64_t l2_writes_;
    uint64_t l3_reads_;
    uint64_t l3_writes_;

    // State
    bool initialized_;
    bool valid_;
    bool power_computed_;
    std::string error_message_;

    // Helper functions
    void runMcPAT();
    void validateConfiguration();
    void createMcPATInput();
    void extractResults();
    std::string generateXMLConfig() const;
};

} // namespace pimid

#endif // PIMID_MCPAT_WRAPPER_H
