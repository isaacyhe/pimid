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
        PCIE               // PCIe controller
        /* 1.11.51 (N5): FULL_SYSTEM is GONE. It was declared and never
         * populated -- the E8-completed rollup lives inside the McPAT fork
         * (processor.cc, correct if anything reads the Processor object
         * directly), but no PIMID-side extraction ever filled this slot, and
         * a declared-but-empty system total invites someone to read zeros.
         * Consumers want getSystemPower()/getTotalArea(), which are real. */
    };

    struct PowerMetrics {
        // Dynamic power (W)
        double subthreshold_leakage;
        double gate_leakage;
        double power_gated_leakage = 0.0;  // 1.11.8: tool's gated endpoint (W)
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
        /* 1.11.50 (L74): does this fabric level sit on the DRAM die? The
         * DRAM-periphery family transform follows this per level instead of
         * blanketing every level in the run: subarray..chip fabrics are on
         * the die; rank/channel/system fabrics are buffer-die or host-board
         * logic per the placement matrix. Default 1 = on die (the single-
         * fabric device case, which is what an unannotated level meant). */
        int on_dram_die;
        // Activity
        uint64_t total_accesses;  // packets at this level
        double duty_cycle;        // derived: total_accesses / total_cycles

        NoCLevelConfig()
            : name("noc"), type(1), horizontal_nodes(4), vertical_nodes(4)
            , input_ports(5), output_ports(5), flit_bits(128)
            , clock_mhz(1000.0), chip_coverage(1.0)
            , on_dram_die(1)
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
        /* 1.11.7 (#85): measured crossing activity + link description.
         * transferred_bytes drives McPAT's byte-based link dynamic;
         * link_clock_mhz replaces the hardwired 350; link_pj_per_bit is
         * chosen from linkEnergyPJPerBit() (cited per-link-type table). */
        double transferred_bytes = 0.0;
        double link_pj_per_bit = 0.0;
        int    link_clock_mhz = 0;   // 0 = legacy 350
        /* 1.11.29: the link CLASS, so the report names what it priced rather
         * than saying "PCIe" for a CXL/NVLink/UALink/interposer run. */
        std::string link_type_name = "pcie";
    };

    /* 1.11.8 (#84): per-component power gating. pg_* mark components the
     * DESCRIBED DESIGN can gate (v7 spec: per-component flag, default
     * false); r_* are measured idle residencies (1 - activePhases/phases).
     * When any pg_* is set, sys.power_gating is emitted so McPAT/CACTI
     * compute the sleep-transistor gated endpoint, and extraction
     * interpolates leak_eff = active*(1-r) + gated*r per component.
     * All false => XML and results identical to 1.11.7 (gate invariant). */
    struct PGSpec {
        bool pg_core = false, pg_noc = false, pg_mc = false;
        double r_core = 0.0, r_noc = 0.0, r_mc = 0.0;
        /* 1.11.18 (audit go-through): the SHARED caches gate on their own
         * no-access signal, not on core retirement -- spec #84 says so, and
         * the counter (pgSharedCacheActivePhases) has been instrumented,
         * exported and parsed since 1.11.8 while L2/L3 were interpolated
         * with r_core. A core can retire from its L1s for long windows with
         * the LLC untouched, so the two residencies are genuinely different
         * numbers. Defaults to r_core when no shared-cache counter was seen
         * (single-level configs), which reproduces the old behavior. */
        double r_shared_cache = 0.0;
        bool   have_shared_cache = false;
    };
    void setPGSpec(const PGSpec& s) { pg_spec_ = s; }

    /* 1.11.7: per-link-type transfer energy, pJ/bit, PHY/link share only
     * (McPAT's ctrl term prices the controller logic separately -- no
     * double count). Sources are ballpark figures from published PHY
     * surveys and vendor claims, flagged for the bounds gate:
     *   pcie_gen3 5.0   pcie_gen4 6.0   pcie_gen5 7.0
     *   cxl       8.4   (gen5 PHY + ~20% coherence-controller delta)
     *   nvlink    1.3   (NVIDIA NVLink4 per-bit claim)
     * Unknown types return -1: caller must reject loudly or use the
     * user override knob (printed k-style). */
    /* 1.11.40 (user ruling): a physical rate is NOT one number. Every energy
     * per bit here is a point on a distribution that moves with process node,
     * operating point, and what the measurement included (PLL and clocking in
     * or out). Carrying a scalar asserts a precision no source supports, and
     * the collapse is where the information was lost: the UCIe entry had its
     * 0.25-0.5 range SOURCED and written into the comment, then returned 0.5.
     *
     * lo/hi are the published bounds. single_point marks an entry where only
     * one figure was found -- the band is then lo==hi and that is a statement
     * about OUR SOURCING, not about the hardware, so it is reported as such
     * rather than passing for a converged value. */
    struct LinkEnergyBand {
        double lo = -1.0;
        double hi = -1.0;
        bool   single_point = false;
        const char* provenance = "";
        bool valid() const { return lo > 0.0 && hi >= lo; }
        double mid() const { return 0.5 * (lo + hi); }
    };
    static LinkEnergyBand linkEnergyBandPJPerBit(const std::string& link_type);

    /* Midpoint of the band. Kept because McPAT's interface takes a scalar;
     * every CALLER that reports a number must report the band alongside it. */
    static double linkEnergyPJPerBit(const std::string& link_type);
    /* 1.11.29: does this link class carry an off-package SerDes? Drives
     * McPAT withPHY, which includes/excludes the SerDes area AND its
     * dynamic term. Interposer is parallel on-package: no SerDes. */
    static bool linkHasSerDes(const std::string& link_type);
    /* 1.11.34 (UCIe): lanes per PHY module for classes with their own module
     * structure -- interposer is 64 (advanced package). <=0 = use the user's
     * configured lane count. */
    static int  linkLanesPerModule(const std::string& link_type);
    /* 1.11.34: every class we model runs a transaction/data-link stack,
     * interposer included -- UCIe's D2D adapter carries PCIe/CXL flit mode. */
    static bool linkHasProtocolStack(const std::string& link_type);
    /* 1.11.29: per-lane SerDes rate (Gb/s) for the link class; <=0 when
     * unknown, which leaves the legacy 4 Gb/s rather than guessing. */
    static double linkSerDesLaneGbps(const std::string& link_type);
    /* 1.11.21 (E1+E2): the DRAM-periphery AREA factor is a ratio between two
     * columns of one CACTI table, and baseline_device names the denominator
     * (0 hp, 1 lstp, 2 lop, 3 lp-dram, 4 comm-dram). Returns false when the
     * composition would be incoherent: an unpopulated column at that node
     * (lp-dram at 22 nm), or a non-hp baseline, for which fd/fl have no
     * derivation. Callers REFUSE on false -- they must not price from it. */
    static bool periphFactorsFor(int dram_table_nm, int logic_node_nm,
                                 int baseline_device, int temp_k,
                                 double& fa, double& fd, double& fl);

    /**
     * Device profile: controls core microarchitecture type in McPAT XML.
     */
    enum class DeviceProfile {
        DEVICE_INORDER,  // machine_type=1, in-order PIM PE (default)
        DEVICE_ALU,      // machine_type=1, compute-unit datapath
        /* 1.9.37: ONE out-of-order model, host and device alike. A separate
         * device variant was briefly added and then withdrawn: an out-of-order
         * PROCESSING ELEMENT is hypothetical -- no shipping in-memory part has
         * one (UPMEM's element is deliberately in-order with many threads to
         * hide latency; the stacked-memory in-bank engine is command-driven
         * SIMD) -- so there is no silicon to calibrate a distinct device variant
         * against, and splitting the enum bought a name without a difference.
         * The out-of-order description remains characterised-core-shaped, which
         * for a hypothetical machine is the most defensible reference available.
         * Note it therefore carries an x86 decode flag; that is a known
         * inaccuracy for a device element, recorded rather than papered over. */
        OOO              // machine_type=0, out-of-order core (host or device)
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
        /* 1.9.36: parametric datapath description, used when the profile is
         * DEVICE_ALU. McPAT has only two core models -- OOO and Inorder
         * (basic_components.h:88) -- and NEITHER describes a processing element
         * that is a datapath rather than a processor. These parameters let the
         * generated description say what the element actually is instead of
         * borrowing an in-order core wholesale. */
        int  pe_lanes;         // 1 = scalar; W = W-wide SIMD
        /* 1.9.40: this is NOT its own configuration knob. It mirrors
         * alu_operand_width (pim.pe.operand_width), the width the TIMING model
         * already charges through ALUCore::operandWidth. 1.9.39 briefly gave the
         * power model a second name for the same physical quantity, parsed
         * separately -- so operand_width: 64 would have produced a 64-bit
         * datapath in timing and a 32-bit one in power. One quantity, one name.
         * Note the power model quantises to 32-bit granularity; a narrower
         * element warns rather than silently disagreeing. */
        int  pe_element_bits;
        bool pe_has_fp;        // three of five kernels are FP32, so normally true
        int  pe_imem_bytes;    // instruction store: ~128 B command file .. 24 KB
                               // spans command-driven to fully programmable, and
                               // gates which kernels can run at all
        int issue_width;
        int num_alus;
        int num_muls;
        int num_fpus;
        /* 1.11.51 (L215/L223): soft-float emulation cycles per FP op, for
         * the FPU-less energy fold (see the FU-stat emission). 0 = none. */
        int fp_emul_cycles = 0;

        /* 1.9.32: reference class. McPAT prices a die against one of two
         * measured populations. The default one is server processors -- the
         * undifferentiated-core term is a curve fitted to Niagara, Niagara2,
         * Merom, Penryn, Prescott and Opteron die photographs, the functional
         * units carry desktop areas, and the wires are top-level global. The
         * other is embedded parts, calibrated to ARM and to Sandia's
         * parametrized processor study.
         *
         * A processing element sitting on a memory die is not a fragment of an
         * Opteron. This flag selects the embedded population for device scope
         * and leaves host scope on the server one, which is what the host
         * actually is. It was never emitted before, so every element in every
         * sweep was priced as server silicon by omission rather than by
         * choice. */
        bool device_scope;

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

        /* 1.11.2: process family of the PE silicon (device scope only).
         *   0 = LOGIC          native McPAT pricing, unchanged
         *   1 = DRAM_PERIPHERY the PE is built from the DRAM die's
         *       peripheral transistors (UPMEM-style). Interim factor model
         *       until 1.11.3 lands a real McPAT device family: the CORE
         *       component is rescaled by factors derived from CACTI's own
         *       22nm device tables (tech_params/22nm.dat, hp vs comm-dram
         *       columns) -- see extractResults() for the derivation.
         * In-class defaults keep every existing construction site (which
         * assigns fields one by one) at native-logic behavior. */
        int process_family = 0;
        double subarray_pitch_factor = 1.0;  // extra area factor at SUBARRAY placement
        // 1.11.3: which CACTI table the DRAM generation class maps to (22 or
        // 32); selects the per-class hp/comm-dram factor set.
        int dram_periph_table_nm = 22;
        /* 1.11.19 (user decisions D2+D3): what this node's memory
         * controllers actually DRIVE. The backend cost model is the same
         * for all three (McPAT's full-MC fit), so the placement ladder
         * stays iso-model; only the driver differs.
         *   NONE       on-die element MC (subarray..chip): no pins at all
         *   INTERPOSER HBM base die / channel tier: TSVs + microbumps
         *   OFFCHIP    rank+/host MC: off-package DQ with ODT
         * Supersedes the 1.11.15 boolean, which could only express
         * "off-chip or embedded" and reached "no PHY" by switching McPAT to
         * a different backend curve entirely. */
        enum class MCPhyTier { NONE, INTERPOSER, OFFCHIP };
        MCPhyTier mc_phy_tier = MCPhyTier::OFFCHIP;

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
            , pe_lanes(1)
            , pe_element_bits(32)
            , pe_has_fp(true)
            , pe_imem_bytes(4096)
            , issue_width(4)
            , num_alus(3)
            , num_muls(1)
            , num_fpus(1)
            , device_scope(false)
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
    /* 1.9.28: MEASURED core activity. Without it the XML is built from fixed
     * fractions of the instruction count (70% int / 10% fp / 10% branch) for
     * every workload, so dynamic power is driven by a constant rather than by
     * what the program executed. Pass 0 for an unavailable counter and that
     * term falls back to the previous fraction. */
    void setMeasuredCoreActivity(uint64_t uops, uint64_t branches,
                                 uint64_t mispredicted);

    /* 1.11.10 (#112): the COUNTED instruction mix, classified by the decoder
     * (x86_decoder.h OpClass) rather than the documented 87.5/12.5 split that
     * stood in for it. All-core totals; the XML divides by core count like
     * every other stat. Zero for any class means "not measured" and that term
     * falls back to the previous fraction, so a core model without a decoder
     * behaves exactly as before. */
    void setMeasuredMix(uint64_t nInt, uint64_t nMul, uint64_t nFp,
                        uint64_t nLd, uint64_t nSt, uint64_t nBr = 0) {
        meas_int_ = nInt; meas_mul_ = nMul; meas_fp_ = nFp;
        meas_ld_ = nLd;   meas_st_ = nSt;   meas_mix_br_ = nBr;
        power_computed_ = false;
    }

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
    uint64_t meas_uops_ = 0;         // 1.9.28: 0 => fall back to fractions
    uint64_t meas_branches_ = 0;
    uint64_t meas_int_ = 0, meas_mul_ = 0, meas_fp_ = 0,
             meas_ld_ = 0, meas_st_ = 0, meas_mix_br_ = 0;   // 1.11.10/.15
    uint64_t meas_mispred_ = 0;
    /* 1.9.29: the measured counters are used only when self-consistent against
     * the instruction count (see the mix construction in the XML writer). This
     * latches the one-time warning when they are not, so a sweep does not emit
     * the same line for every node of every cell. */
    mutable bool warned_mix_ = false;
    /* 1.11.52 (audit C016/C007): latch + emitter for the unsourced-fraction
     * warning. Three blocks substitute invented activity when nothing was
     * measured, and all three were silent. */
    mutable bool warned_unsourced_mix_ = false;
    void warnUnsourcedMix(const char* which, const char* frac) const;
    /* 1.11.52 (audit C016/C007): latch for the unsourced-fraction warning --
     * three blocks substitute invented activity when nothing was measured,
     * and all three were silent. */
    mutable bool warned_narrow_datapath_ = false;  // 1.9.40
    /* 1.9.36: per-core intra-core power split, transported from the forked child
     * (which alone holds the model object). Diagnostic only -- nothing consumes
     * these -- but they are what makes the ALU-versus-core error MEASURABLE
     * before an ALU model replaces it. */
    double core_ifu_w_ = 0.0, core_lsu_w_ = 0.0, core_mmu_w_ = 0.0;
    double core_exu_w_ = 0.0, core_pipe_w_ = 0.0, core_undiff_w_ = 0.0;
  public:
    struct CoreBreakdown { double ifu, lsu, mmu, exu, corepipe, undiff; };
    CoreBreakdown getCoreBreakdown() const {
        return { core_ifu_w_, core_lsu_w_, core_mmu_w_,
                 core_exu_w_, core_pipe_w_, core_undiff_w_ };
    }
  private:

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
    /* 1.11.29: the LINK controller area. McPAT computes it
     * (iocontrollers.cc: (ctrl_area + SerDer_area)/8*num_channels) and
     * getComponentArea had no PCIE case, so it fell through to 0 -- the
     * area was calculated and then dropped, including from the system
     * total. Measured: the controller carries 0.0037 W of leakage per
     * end, so it is present; only its area was invisible. */
    double mcpat_pcie_area_mm2_ = 0.0;
    // 1.11.4: scaled/unscaled core power ratio under the DRAM-periphery
    // family; the CoreBreakdown block weights are multiplied by this so the
    // printed split stays consistent with the scaled core total.
    double fam_core_power_ratio_ = 1.0;
    PGSpec pg_spec_;   // 1.11.8
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
