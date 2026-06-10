#ifndef PIMID_MCPAT_WRAPPER_H
#define PIMID_MCPAT_WRAPPER_H

#include "common/types.h"
#include <string>
#include <memory>
#include <map>
#include <vector>

// Forward declarations for McPAT types
class ParseXML;
class Processor;

namespace pimid {

/**
 * Wrapper class that adapts McPAT to PIMID's power model interface
 * Provides integrated power, area, and timing modeling for:
 * - CPU cores
 * - Caches (L1, L2, L3)
 * - NoCs (Network-on-Chip) — N instances, one per hierarchy level
 * - Memory controllers
 * - PCIe (for co-sim transfers)
 */
class McPATWrapper {
public:
    enum class ComponentType {
        CORE,              // CPU core
        L1_CACHE,          // L1 cache
        L2_CACHE,          // L2 cache
        L3_CACHE,          // L3 cache (shared)
        NOC,               // Network-on-Chip (aggregate of all levels)
        MEMORY_CONTROLLER, // Memory controller
        PCIE,              // PCIe controller
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

    /**
     * Per-hierarchy-level NoC configuration for McPAT.
     * Each level becomes a separate system.nocN component in the XML.
     */
    struct NoCLevelConfig {
        std::string name;         // e.g. "L0_PE_mesh", "L1_bank_bus"
        int type;                 // 0=bus, 1=router-based NoC
        int horizontal_nodes;
        int vertical_nodes;
        int input_ports;
        int output_ports;
        int flit_bits;
        double clock_mhz;
        double chip_coverage;     // fraction of chip area
        // Activity
        uint64_t total_accesses;  // packets at this level
        double duty_cycle;        // derived: total_accesses / total_cycles

        NoCLevelConfig()
            : name("noc"), type(1), horizontal_nodes(4), vertical_nodes(4)
            , input_ports(5), output_ports(5), flit_bits(128)
            , clock_mhz(1000.0), chip_coverage(1.0)
            , total_accesses(0), duty_cycle(0.0)
        {}
    };

    /**
     * Memory controller technology parameters for McPAT.
     * Auto-derived from memory technology or overridden via YAML.
     */
    struct MCTechParams {
        int peak_transfer_rate = 3200;  // MT/s
        int databus_width = 64;         // bits
        int number_ranks = 2;
        int number_mcs = 1;
    };

    /**
     * PCIe statistics for co-sim transfer power modeling.
     */
    struct PCIeStats {
        int number_units = 0;
        int num_channels = 0;
        double duty_cycle = 0.0;
        double total_load_perc = 0.0;
    };

    /**
     * Device profile: controls core microarchitecture type in McPAT XML.
     */
    enum class DeviceProfile {
        DEVICE_INORDER,  // machine_type=1, in-order PIM PE (default)
        DEVICE_ALU,      // machine_type=1, no caches
        HOST_OOO         // machine_type=0, x86 OOO host core
    };

    /**
     * NoC activity statistics from Garnet simulation (legacy, kept for compatibility).
     * Used when setNoCLevels() is not called.
     */
    struct NoCActivityStats {
        uint64_t total_packets;
        uint64_t total_flits;
        uint64_t total_hops;
        uint64_t buffer_reads;
        uint64_t buffer_writes;
        uint64_t crossbar_traversals;
        uint64_t arbiter_events;
        uint64_t link_traversals;
        uint64_t total_cycles;
        double clock_mhz;

        NoCActivityStats()
            : total_packets(0), total_flits(0), total_hops(0)
            , buffer_reads(0), buffer_writes(0)
            , crossbar_traversals(0), arbiter_events(0)
            , link_traversals(0), total_cycles(0), clock_mhz(1000.0)
        {}
    };

    struct SystemConfig {
        // Core parameters
        int num_cores;
        double core_clock_mhz;
        int pipeline_depth;
        int issue_width;
        int num_alus;
        int num_muls;
        int num_fpus;

        // Cache parameters
        uint64_t l1i_size_bytes;
        uint64_t l1d_size_bytes;
        uint64_t l2_size_bytes;
        uint64_t l3_size_bytes;

        // Memory parameters
        int num_memory_controllers;
        double mc_clock_mhz;

        // NoC parameters (used when noc_levels_ is empty)
        bool has_noc;
        int noc_topology;  // 0=mesh, 1=crossbar, 2=bus
        int noc_num_routers;
        int noc_num_rows;
        int noc_num_cols;
        int noc_flit_size_bits;
        int noc_input_ports;
        int noc_output_ports;
        int noc_vcs_per_vnet;
        int noc_vc_buffer_size;  // In flits
        double noc_clock_mhz;

        // Technology
        int tech_node_nm;
        int temperature_k;

        // McPAT system-level parameters (exposed for architecture exploration)
        int device_type;                    // 0=HP, 1=LSTP, 2=LOP
        int longer_channel_device;          // 0 or 1
        int number_hardware_threads;        // threads per core
        int interconnect_projection_type;   // 0=aggressive, 1=conservative

        // XML configuration file (optional)
        std::string xml_file;

        SystemConfig()
            : num_cores(4)
            , core_clock_mhz(2000.0)
            , pipeline_depth(14)
            , issue_width(4)
            , num_alus(3)
            , num_muls(1)
            , num_fpus(1)
            , l1i_size_bytes(32 * 1024)
            , l1d_size_bytes(32 * 1024)
            , l2_size_bytes(256 * 1024)
            , l3_size_bytes(8 * 1024 * 1024)
            , num_memory_controllers(1)
            , mc_clock_mhz(800.0)
            , has_noc(true)
            , noc_topology(0)
            , noc_num_routers(16)
            , noc_num_rows(4)
            , noc_num_cols(4)
            , noc_flit_size_bits(128)
            , noc_input_ports(5)
            , noc_output_ports(5)
            , noc_vcs_per_vnet(4)
            , noc_vc_buffer_size(4)
            , noc_clock_mhz(1000.0)
            , tech_node_nm(22)
            , temperature_k(350)
            , device_type(0)
            , longer_channel_device(1)
            , number_hardware_threads(1)
            , interconnect_projection_type(0)
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

    // Split cache stat setters — use actual ZSim counters, not combined reads/writes
    void setL1IAccesses(uint64_t reads, uint64_t read_misses);
    void setL1DAccesses(uint64_t reads, uint64_t writes, uint64_t read_misses, uint64_t write_misses);
    void setL2Accesses(uint64_t reads, uint64_t writes, uint64_t read_misses, uint64_t write_misses);
    void setL3Accesses(uint64_t reads, uint64_t writes, uint64_t read_misses, uint64_t write_misses);

    // Memory controller stats from ZSim (rd/wr counters)
    void setMemControllerAccesses(uint64_t reads, uint64_t writes);

    // MC technology parameters (auto-derived or YAML override)
    void setMCTechParams(const MCTechParams& params);

    // N NoC levels (one per hierarchy level)
    void setNoCLevels(const std::vector<NoCLevelConfig>& levels);

    // Set NoC activity from Garnet simulation (legacy fallback)
    void setNoCActivity(const NoCActivityStats& stats);

    // PCIe stats for co-sim transfers
    void setPCIeStats(const PCIeStats& stats);

    // Device profile (host OOO vs device in-order vs ALU)
    void setDeviceProfile(DeviceProfile profile);

    // Run power analysis
    void computePower();

    // Query power results by component
    PowerMetrics getComponentPower(ComponentType component) const;
    PowerMetrics getSystemPower() const;

    // Specific component queries
    double getCorePower() const;           // Total power for all cores
    double getCachePower() const;          // Total cache hierarchy power
    double getMemoryControllerPower() const;
    double getNoCPower() const;

    // Per-NoC-level power breakdown
    const std::vector<PowerMetrics>& getNoCLevelPower() const { return noc_level_power_; }

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
    void printSummaryLine() const;

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

    // Split cache stats
    uint64_t l1i_reads_;
    uint64_t l1i_read_misses_;
    uint64_t l1d_reads_;
    uint64_t l1d_writes_;
    uint64_t l1d_read_misses_;
    uint64_t l1d_write_misses_;
    uint64_t l2_reads_;
    uint64_t l2_writes_;
    uint64_t l2_read_misses_;
    uint64_t l2_write_misses_;
    uint64_t l3_reads_;
    uint64_t l3_writes_;
    uint64_t l3_read_misses_;
    uint64_t l3_write_misses_;

    // MC stats
    uint64_t mc_reads_;
    uint64_t mc_writes_;

    // MC technology params
    MCTechParams mc_tech_;

    // NoC per-level configs
    std::vector<NoCLevelConfig> noc_levels_;

    // NoC activity stats (legacy fallback)
    NoCActivityStats noc_activity_;

    // PCIe stats
    PCIeStats pcie_stats_;

    // Device profile
    DeviceProfile device_profile_;

    // Per-NoC-level power results
    std::vector<PowerMetrics> noc_level_power_;

    // Peak power (design-time, from power.readOp.dynamic + leakage)
    double peak_power_ = 0.0;

    // McPAT area results (mm^2), populated by extractResults()
    double mcpat_core_area_mm2_ = 0.0;
    double mcpat_l2_area_mm2_ = 0.0;
    double mcpat_l3_area_mm2_ = 0.0;
    double mcpat_noc_area_mm2_ = 0.0;
    double mcpat_mc_area_mm2_ = 0.0;
    double mcpat_total_area_mm2_ = 0.0;

    // State
    bool initialized_;
    bool valid_;
    bool power_computed_;
    bool user_provided_xml_;
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
