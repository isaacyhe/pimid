/**
 * @file test_pim_granularity.cpp
 * @brief PIM Granularity Comparison: Subarray vs Bank vs Chip vs Rank vs MC vs CPU
 *
 * This test compares different PIM placement granularities with EQUAL compute power
 * but DIFFERENT data movement costs based on DDR4 DRAM architecture.
 *
 * DDR4 DRAM Hierarchy:
 * ┌─────────────────────────────────────────────────────────────┐
 * │ Memory Controller (MC)                                      │
 * │  ├─ Rank 0                                                  │
 * │  │   ├─ Chip 0                                              │
 * │  │   │   ├─ Bank Group 0                                    │
 * │  │   │   │   ├─ Bank 0                                      │
 * │  │   │   │   │   ├─ Subarray 0 (512 rows × 1024 columns)   │
 * │  │   │   │   │   ├─ Subarray 1                             │
 * │  │   │   │   │   └─ ...                                    │
 * │  │   │   │   ├─ Bank 1                                      │
 * │  │   │   │   └─ Bank 2, 3                                   │
 * │  │   │   └─ Bank Group 1, 2, 3                              │
 * │  │   └─ Chip 1-7 (x8 organization)                          │
 * │  └─ Rank 1                                                  │
 * └─────────────────────────────────────────────────────────────┘
 *
 * Key Insight: ALL compute devices have EQUAL performance,
 *              but data movement costs differ dramatically!
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <string>

#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN "\033[0;36m"
#define BOLD "\033[1m"
#define NC "\033[0m"

//=============================================================================
// DDR4 DRAM Architecture Constants
//=============================================================================
namespace DDR4 {
    // Physical organization
    const int SUBARRAYS_PER_BANK = 4;
    const int BANKS_PER_BANK_GROUP = 4;
    const int BANK_GROUPS_PER_CHIP = 4;
    const int CHIPS_PER_RANK = 8;        // x8 device
    const int RANKS_PER_MC = 2;

    // Capacity (typical DDR4-2400 8GB DIMM)
    const size_t SUBARRAY_SIZE_KB = 512;  // 512KB per subarray
    const size_t BANK_SIZE_MB = 2;        // 4 subarrays × 512KB
    const size_t CHIP_SIZE_MB = 128;      // 16 banks × 2MB
    const size_t RANK_SIZE_GB = 1;        // 8 chips × 128MB
    const size_t DIMM_SIZE_GB = 2;        // 2 ranks × 1GB

    // Timing (DDR4-2400, simplified)
    const double tRCD_ns = 13.32;         // RAS-to-CAS delay
    const double tCAS_ns = 13.32;         // CAS latency
    const double tRP_ns = 13.32;          // Precharge time
    const double tRAS_ns = 32.0;          // Row activation time
    const double tBurst_ns = 3.33;        // 8-beat burst @ 2400 MT/s

    // Data transfer latencies (hierarchical)
    const double SUBARRAY_LATENCY_ns = tRCD_ns + tCAS_ns;           // ~26ns (open row)
    const double BANK_LATENCY_ns = tRP_ns + tRCD_ns + tCAS_ns;     // ~40ns (close+open)
    const double BANK_GROUP_LATENCY_ns = 50.0;                      // Bank group switching
    const double CHIP_LATENCY_ns = 60.0;                            // On-chip routing
    const double RANK_LATENCY_ns = 80.0;                            // Rank switching
    const double MC_LATENCY_ns = 100.0;                             // Memory controller
    const double CPU_LATENCY_ns = 100.0;                            // MC + System bus

    // Bandwidth (aggregate, GB/s)
    const double SUBARRAY_BW_GBs = 2.4;                             // Single subarray bandwidth
    const double BANK_BW_GBs = 4.8;                                 // Bank parallelism
    const double BANK_GROUP_BW_GBs = 9.6;                           // Bank group parallelism
    const double CHIP_BW_GBs = 19.2;                                // Full chip
    const double RANK_BW_GBs = 38.4;                                // Full rank (8 chips × 4.8)
    const double MC_BW_GBs = 76.8;                                  // 2 ranks
    const double CPU_BW_GBs = 76.8;                                 // Same as MC

    // Energy per data movement (pJ per byte, hierarchical)
    const double SUBARRAY_ENERGY_pJ = 1.0;                          // Sense amp energy
    const double BANK_ENERGY_pJ = 2.0;                              // + bank switching
    const double BANK_GROUP_ENERGY_pJ = 3.0;                        // + bank group mux
    const double CHIP_ENERGY_pJ = 5.0;                              // + on-chip routing
    const double RANK_ENERGY_pJ = 10.0;                             // + rank selection + I/O
    const double MC_ENERGY_pJ = 15.0;                               // + memory controller
    const double CPU_ENERGY_pJ = 20.0;                              // + system bus + LLC

    // Port Bitwidth Constraints (CRITICAL CORRECTION!)
    // INSIDE DRAM: Banks and bank groups have NARROW 8-16 bit ports!
    // Only at RANK/DIMM level do we get wide 64-bit interfaces!
    const int BANK_PORT_BITS = 8;                                   // 8-bit bank port (typical DDR4 internal)
    const int BANK_GROUP_PORT_BITS = 16;                            // ~16-bit bank group (limited by internal routing)
    const int CHIP_IO_BITS = 8;                                     // x8 device external I/O (1 byte per cycle)
    const int RANK_DATA_BITS = 64;                                  // 8 chips × 8 bits = 64-bit rank interface
    const int MC_DATA_BITS = 64;                                    // 64-bit memory controller (single channel)

    // Port bandwidth at DDR4-2400 (2.4 GT/s effective, 1.2 GHz DDR clock)
    const double CLOCK_GHz = 1.2;
    const double BANK_PORT_BW_GBs = (BANK_PORT_BITS / 8.0) * CLOCK_GHz;         // 1.2 GB/s per bank (NARROW!)
    const double BANK_GROUP_PORT_BW_GBs = (BANK_GROUP_PORT_BITS / 8.0) * CLOCK_GHz;  // 2.4 GB/s per bank group
    const double CHIP_IO_BW_GBs = (CHIP_IO_BITS / 8.0) * CLOCK_GHz;            // 1.2 GB/s per chip (x8)
    const double RANK_BW_GBs_PORT = (RANK_DATA_BITS / 8.0) * CLOCK_GHz;        // 9.6 GB/s per rank (WIDE!)
    const double MC_BW_GBs_PORT = (MC_DATA_BITS / 8.0) * CLOCK_GHz;            // 9.6 GB/s for MC
}

//=============================================================================
// PIM Architecture Definitions
//=============================================================================
struct PIMArchitecture {
    std::string name;
    std::string description;

    // Equal compute resources (normalized)
    int num_compute_units;        // All architectures have SAME number
    double compute_power_gflops;  // All have SAME compute power

    // Different data movement characteristics
    double data_latency_ns;       // Latency to access data
    double aggregate_bw_GBs;      // Aggregate bandwidth (ideal, without port contention)
    double energy_per_byte_pJ;    // Energy cost per byte

    // Physical constraints
    int total_units;              // Total number in system
    size_t local_memory_kb;       // Local memory per unit

    // Port contention modeling (CRITICAL!)
    int units_per_port;           // How many PIM units share one port
    double port_bw_GBs;           // Bandwidth of the shared port
    double effective_bw_GBs;      // Actual bandwidth after contention

    // Color for visualization
    const char* color;
};

//=============================================================================
// Create Architecture Definitions
//=============================================================================
std::vector<PIMArchitecture> createArchitectures() {
    std::vector<PIMArchitecture> archs;

    // For fair comparison: 64 total compute units across ALL architectures
    const int TOTAL_COMPUTE_UNITS = 64;
    const double COMPUTE_POWER_PER_UNIT = 2.0;  // 2 GFLOPS per unit

    // 1. Subarray-level PIM
    // 4 subarrays per bank SHARE the bank's 8-bit port (1.2 GB/s) → SEVERE CONTENTION!
    archs.push_back({
        "Subarray-PIM",
        "Compute at each subarray (finest granularity)",
        TOTAL_COMPUTE_UNITS / 64,  // 1 unit per subarray
        COMPUTE_POWER_PER_UNIT,
        DDR4::SUBARRAY_LATENCY_ns,
        DDR4::SUBARRAY_BW_GBs * 64,  // 64 subarrays in parallel (IDEAL, if no port limits)
        DDR4::SUBARRAY_ENERGY_pJ,
        64,  // 64 subarrays in system
        DDR4::SUBARRAY_SIZE_KB,
        4,   // 4 subarrays share 1 bank port (CRITICAL!)
        DDR4::BANK_PORT_BW_GBs,  // 1.2 GB/s shared by 4 subarrays
        DDR4::BANK_PORT_BW_GBs / 4,  // 0.3 GB/s effective per subarray (TINY!)
        CYAN
    });

    // 2. Bank-level PIM (Multiple PEs per bank)
    // Each bank has 8-bit port (1.2 GB/s) - NARROW internal port!
    archs.push_back({
        "Bank-PIM",
        "Compute at each bank (4 PEs per bank)",
        TOTAL_COMPUTE_UNITS / 16,  // 4 units per bank
        COMPUTE_POWER_PER_UNIT,
        DDR4::BANK_LATENCY_ns,
        DDR4::BANK_BW_GBs * 16,    // 16 banks in parallel (IDEAL if no port limits)
        DDR4::BANK_ENERGY_pJ,
        16,  // 16 banks total
        DDR4::BANK_SIZE_MB * 1024,
        1,   // Each bank has dedicated 8-bit port (no inter-bank contention)
        DDR4::BANK_PORT_BW_GBs,  // 1.2 GB/s per bank (NARROW!)
        DDR4::BANK_PORT_BW_GBs,  // 1.2 GB/s effective per bank
        BLUE
    });

    // 3. Bank Group-level PIM
    // Each bank group has ~16-bit internal port (2.4 GB/s) - still narrow!
    archs.push_back({
        "BankGroup-PIM",
        "Compute at each bank group",
        TOTAL_COMPUTE_UNITS / 4,   // 16 units per bank group
        COMPUTE_POWER_PER_UNIT,
        DDR4::BANK_GROUP_LATENCY_ns,
        DDR4::BANK_GROUP_BW_GBs * 4,  // 4 bank groups in parallel (IDEAL)
        DDR4::BANK_GROUP_ENERGY_pJ,
        4,   // 4 bank groups total
        DDR4::BANK_SIZE_MB * 4 * 1024,
        1,   // Each bank group has dedicated ~16-bit port
        DDR4::BANK_GROUP_PORT_BW_GBs,  // 2.4 GB/s per bank group (still narrow!)
        DDR4::BANK_GROUP_PORT_BW_GBs,  // 2.4 GB/s effective
        MAGENTA
    });

    // 4. Chip-level PIM
    // All 16 banks in chip share chip I/O (x8 = 1.2 GB/s) → SEVERE BOTTLENECK!
    archs.push_back({
        "Chip-PIM",
        "Compute at each chip (HBM-like)",
        TOTAL_COMPUTE_UNITS / 2,   // 32 units per chip
        COMPUTE_POWER_PER_UNIT,
        DDR4::CHIP_LATENCY_ns,
        DDR4::CHIP_BW_GBs * 2,     // 2 chips in parallel (IDEAL)
        DDR4::CHIP_ENERGY_pJ,
        2,   // 2 chips (simplified)
        DDR4::CHIP_SIZE_MB * 1024,
        16,  // 16 banks share chip I/O (SEVERE contention!)
        DDR4::CHIP_IO_BW_GBs,  // Only 1.2 GB/s chip I/O!
        DDR4::CHIP_IO_BW_GBs,  // 1.2 GB/s total for entire chip
        YELLOW
    });

    // 5. Rank-level PIM
    // 8 chips combine to form rank interface (64-bit = 9.6 GB/s)
    archs.push_back({
        "Rank-PIM",
        "Compute at each rank (DIMM-level)",
        TOTAL_COMPUTE_UNITS / 2,   // 32 units per rank
        COMPUTE_POWER_PER_UNIT,
        DDR4::RANK_LATENCY_ns,
        DDR4::RANK_BW_GBs * 2,     // 2 ranks in parallel (IDEAL)
        DDR4::RANK_ENERGY_pJ,
        2,   // 2 ranks
        DDR4::RANK_SIZE_GB * 1024 * 1024,
        8,   // 8 chips share rank interface
        DDR4::RANK_BW_GBs_PORT,  // 9.6 GB/s per rank
        DDR4::RANK_BW_GBs_PORT,  // 9.6 GB/s total
        GREEN
    });

    // 6. MC-wide PIM
    // MC has 64-bit interface (9.6 GB/s) - FIRST wide interface!
    archs.push_back({
        "MC-PIM",
        "Compute at memory controller",
        TOTAL_COMPUTE_UNITS,       // All 64 units at MC
        COMPUTE_POWER_PER_UNIT,
        DDR4::MC_LATENCY_ns,
        DDR4::MC_BW_GBs,           // Full MC bandwidth
        DDR4::MC_ENERGY_pJ,
        1,   // 1 memory controller
        DDR4::DIMM_SIZE_GB * 2 * 1024 * 1024,
        1,   // MC has dedicated 64-bit port
        DDR4::MC_BW_GBs_PORT,  // 9.6 GB/s MC bandwidth (64-bit)
        DDR4::MC_BW_GBs_PORT,  // 9.6 GB/s total
        BLUE
    });

    // 7. Traditional CPU (for comparison)
    // CPU accesses memory through MC (64-bit interface)
    archs.push_back({
        "CPU",
        "Traditional CPU with remote memory",
        TOTAL_COMPUTE_UNITS,       // Same 64 cores
        COMPUTE_POWER_PER_UNIT,
        DDR4::CPU_LATENCY_ns,
        DDR4::CPU_BW_GBs,
        DDR4::CPU_ENERGY_pJ,
        1,   // 1 CPU socket
        32 * 1024,  // 32MB LLC
        1,   // Single CPU → MC path
        DDR4::MC_BW_GBs_PORT,  // 9.6 GB/s (64-bit MC interface)
        DDR4::MC_BW_GBs_PORT,  // 9.6 GB/s
        RED
    });

    return archs;
}

//=============================================================================
// Workload Simulation
//=============================================================================
struct WorkloadResult {
    double execution_time_us;
    double total_energy_mJ;
    double data_movement_time_us;
    double compute_time_us;
    double effective_bandwidth_GBs;
    size_t data_transferred_bytes;
};

WorkloadResult simulateVectorAdd(const PIMArchitecture& arch, size_t vector_size_elements) {
    WorkloadResult result = {};

    const size_t element_size = 8;  // 8 bytes (double precision)
    const size_t total_data_bytes = vector_size_elements * element_size * 3;  // Read A, B, write C

    // Compute time - calculated PER UNIT (partition)
    const double ops_per_element = 1;  // One addition
    const double total_ops = vector_size_elements * ops_per_element;
    const double ops_per_unit = total_ops / arch.total_units;  // Each partition processes its share
    const double total_compute_gflops = arch.num_compute_units * arch.compute_power_gflops;  // Compute power per partition
    result.compute_time_us = (ops_per_unit / (total_compute_gflops * 1e9)) * 1e6;  // Time for one partition

    // Data movement time (DIFFERS by architecture!)
    result.data_transferred_bytes = total_data_bytes;

    // Account for parallelism in data movement
    const double data_per_unit = (double)total_data_bytes / arch.total_units;

    // Data movement latency + transfer time
    // CRITICAL: Use effective_bw_GBs which accounts for port contention!
    const double transfer_time_per_unit = (data_per_unit / 1e9) / arch.effective_bw_GBs * 1e6;  // us
    const double latency_overhead = arch.data_latency_ns / 1000.0;  // Convert to us

    result.data_movement_time_us = latency_overhead + transfer_time_per_unit;

    // Total execution time
    // For PIM: compute and data movement overlap (pipelined)
    // For CPU: they are more sequential
    bool is_cpu = (arch.name == "CPU");
    if (is_cpu) {
        // CPU: data fetch, compute, write back (sequential)
        result.execution_time_us = result.data_movement_time_us + result.compute_time_us;
    } else {
        // PIM: overlapped execution (compute while data is being moved)
        result.execution_time_us = std::max(result.data_movement_time_us, result.compute_time_us);
    }

    // Energy calculation
    result.total_energy_mJ = (total_data_bytes * arch.energy_per_byte_pJ) / 1e9;  // Convert pJ to mJ

    // Add compute energy (same for all)
    const double compute_energy_mJ = total_compute_gflops * result.compute_time_us * 0.05;  // 50mW per GFLOPS
    result.total_energy_mJ += compute_energy_mJ;

    // Effective bandwidth
    result.effective_bandwidth_GBs = (total_data_bytes / 1e9) / (result.execution_time_us / 1e6);

    return result;
}

//=============================================================================
// Visualization and Analysis
//=============================================================================
void printArchitectureComparison() {
    std::cout << "\n" << CYAN << BOLD << "╔═══════════════════════════════════════════════════════════╗" << NC << "\n";
    std::cout << CYAN << BOLD << "║        DDR4 DRAM PIM Granularity Comparison               ║" << NC << "\n";
    std::cout << CYAN << BOLD << "║        Equal Compute Power, Different Data Movement      ║" << NC << "\n";
    std::cout << CYAN << BOLD << "╚═══════════════════════════════════════════════════════════╝" << NC << "\n\n";

    auto archs = createArchitectures();

    // Architecture overview
    std::cout << YELLOW << "DDR4 Architecture Overview:" << NC << "\n";
    std::cout << "  Memory Controller → Rank → Chip → Bank Group → Bank → Subarray\n";
    std::cout << "  └─ Each level has different data movement cost\n\n";

    // Show physical organization
    std::cout << MAGENTA << "Physical Organization:" << NC << "\n";
    std::cout << "  Subarrays per Bank:      " << DDR4::SUBARRAYS_PER_BANK << "\n";
    std::cout << "  Banks per Bank Group:    " << DDR4::BANKS_PER_BANK_GROUP << "\n";
    std::cout << "  Bank Groups per Chip:    " << DDR4::BANK_GROUPS_PER_CHIP << "\n";
    std::cout << "  Chips per Rank:          " << DDR4::CHIPS_PER_RANK << "\n";
    std::cout << "  Ranks per MC:            " << DDR4::RANKS_PER_MC << "\n\n";

    // Architecture comparison table
    std::cout << CYAN << BOLD << "Architecture Comparison (All have 64 compute units @ 2 GFLOPS each):" << NC << "\n";
    std::cout << std::string(125, '-') << "\n";
    std::cout << std::setw(15) << "Architecture"
              << std::setw(12) << "Units"
              << std::setw(15) << "Local Mem"
              << std::setw(18) << "Data Latency"
              << std::setw(15) << "Bandwidth"
              << std::setw(18) << "Energy/Byte"
              << std::setw(30) << "Key Characteristic"
              << "\n";
    std::cout << std::string(125, '-') << "\n";

    for (const auto& arch : archs) {
        std::cout << arch.color << std::setw(15) << arch.name << NC
                  << std::setw(12) << arch.total_units;

        // Format local memory
        if (arch.local_memory_kb < 1024) {
            std::cout << std::setw(12) << arch.local_memory_kb << " KB";
        } else if (arch.local_memory_kb < 1024 * 1024) {
            std::cout << std::setw(12) << (arch.local_memory_kb / 1024) << " MB";
        } else {
            std::cout << std::setw(12) << (arch.local_memory_kb / (1024 * 1024)) << " GB";
        }

        std::cout << std::setw(14) << arch.data_latency_ns << " ns"
                  << std::setw(12) << arch.aggregate_bw_GBs << " GB/s"
                  << std::setw(14) << arch.energy_per_byte_pJ << " pJ/B";

        // Key characteristic
        if (arch.name == "Subarray-PIM") std::cout << "     Lowest latency, highest parallelism";
        else if (arch.name == "Bank-PIM") std::cout << "     Good balance of locality";
        else if (arch.name == "BankGroup-PIM") std::cout << "     Moderate granularity";
        else if (arch.name == "Chip-PIM") std::cout << "     Similar to HBM-PIM";
        else if (arch.name == "Rank-PIM") std::cout << "     DIMM-level processing";
        else if (arch.name == "MC-PIM") std::cout << "     Near-memory processing";
        else if (arch.name == "CPU") std::cout << "     Highest latency, data movement";

        std::cout << "\n";
    }
    std::cout << std::string(125, '-') << "\n\n";

    // Port Contention Analysis (CRITICAL!)
    std::cout << RED << BOLD << "⚠️  PORT CONTENTION ANALYSIS (Critical Bottleneck!)" << NC << "\n";
    std::cout << std::string(140, '-') << "\n";
    std::cout << std::setw(15) << "Architecture"
              << std::setw(18) << "Units per Port"
              << std::setw(18) << "Port BW (GB/s)"
              << std::setw(22) << "Effective BW (GB/s)"
              << std::setw(20) << "Contention Impact"
              << std::setw(35) << "Bottleneck"
              << "\n";
    std::cout << std::string(140, '-') << "\n";

    for (const auto& arch : archs) {
        double contention_factor = arch.port_bw_GBs / arch.effective_bw_GBs;
        std::string impact;
        std::string bottleneck;

        if (arch.units_per_port == 1) {
            impact = "None (dedicated)";
            bottleneck = "No contention";
        } else if (arch.units_per_port <= 4) {
            impact = "Low (" + std::to_string(arch.units_per_port) + "x sharing)";
            bottleneck = "Manageable serialization";
        } else if (arch.units_per_port <= 8) {
            impact = "Medium (" + std::to_string(arch.units_per_port) + "x sharing)";
            bottleneck = "Significant serialization";
        } else {
            impact = "SEVERE (" + std::to_string(arch.units_per_port) + "x sharing)";
            bottleneck = "CRITICAL BOTTLENECK!";
        }

        std::cout << arch.color << std::setw(15) << arch.name << NC
                  << std::setw(18) << arch.units_per_port
                  << std::setw(18) << std::fixed << std::setprecision(1) << arch.port_bw_GBs
                  << std::setw(22) << arch.effective_bw_GBs
                  << std::setw(20) << impact
                  << std::setw(35) << bottleneck
                  << "\n";
    }
    std::cout << std::string(140, '-') << "\n";
    std::cout << YELLOW << "Note: Subarray-PIM suffers from 4x port contention (4 subarrays share 1 bank port)" << NC << "\n";
    std::cout << YELLOW << "      Chip-PIM suffers from SEVERE contention (16 banks share tiny x8 I/O)" << NC << "\n";
    std::cout << YELLOW << "      Bank-PIM has dedicated ports → NO contention!" << NC << "\n\n";
}

void runWorkloadComparison() {
    auto archs = createArchitectures();

    std::cout << CYAN << BOLD << "\n╔═══════════════════════════════════════════════════════════╗" << NC << "\n";
    std::cout << CYAN << BOLD << "║          Workload: Vector Addition (16 MB data)           ║" << NC << "\n";
    std::cout << CYAN << BOLD << "╚═══════════════════════════════════════════════════════════╝" << NC << "\n\n";

    const size_t vector_size = 2 * 1024 * 1024;  // 2M elements × 8 bytes = 16 MB

    std::cout << "Workload: C[i] = A[i] + B[i] for i = 0 to " << vector_size << "\n";
    std::cout << "Data size: " << (vector_size * 8 * 3) / (1024.0 * 1024.0) << " MB (read A, B; write C)\n";
    std::cout << "Compute: " << vector_size << " additions @ 2 GFLOPS per unit\n\n";

    std::cout << std::string(140, '-') << "\n";
    std::cout << std::setw(15) << "Architecture"
              << std::setw(15) << "Total Time"
              << std::setw(15) << "Data Move"
              << std::setw(15) << "Compute"
              << std::setw(15) << "Total Energy"
              << std::setw(15) << "Eff. BW"
              << std::setw(20) << "Speedup vs CPU"
              << std::setw(20) << "Energy Eff. vs CPU"
              << "\n";
    std::cout << std::string(140, '-') << "\n";

    // Get CPU baseline
    WorkloadResult cpu_result;
    for (const auto& arch : archs) {
        if (arch.name == "CPU") {
            cpu_result = simulateVectorAdd(arch, vector_size);
            break;
        }
    }

    // Run all architectures
    std::vector<WorkloadResult> results;
    for (const auto& arch : archs) {
        auto result = simulateVectorAdd(arch, vector_size);
        results.push_back(result);

        double speedup = cpu_result.execution_time_us / result.execution_time_us;
        double energy_efficiency = cpu_result.total_energy_mJ / result.total_energy_mJ;

        std::cout << arch.color << std::setw(15) << arch.name << NC
                  << std::setw(12) << std::fixed << std::setprecision(2) << result.execution_time_us << " μs"
                  << std::setw(12) << result.data_movement_time_us << " μs"
                  << std::setw(12) << result.compute_time_us << " μs"
                  << std::setw(12) << result.total_energy_mJ << " mJ"
                  << std::setw(12) << result.effective_bandwidth_GBs << " GB/s"
                  << std::setw(17) << speedup << "x"
                  << std::setw(17) << energy_efficiency << "x"
                  << "\n";
    }
    std::cout << std::string(140, '-') << "\n\n";
}

void analyzeResults() {
    std::cout << YELLOW << BOLD << "\n╔═══════════════════════════════════════════════════════════╗" << NC << "\n";
    std::cout << YELLOW << BOLD << "║                    Key Insights                            ║" << NC << "\n";
    std::cout << YELLOW << BOLD << "╚═══════════════════════════════════════════════════════════╝" << NC << "\n\n";

    std::cout << GREEN << "1. Compute Power is EQUAL across all architectures:" << NC << "\n";
    std::cout << "   └─ All have 64 compute units @ 2 GFLOPS = 128 GFLOPS total\n";
    std::cout << "   └─ Pure compute time is IDENTICAL\n\n";

    std::cout << GREEN << "2. Data Movement is DIFFERENT:" << NC << "\n";
    std::cout << "   └─ Subarray-PIM: Data already in subarray (26ns latency)\n";
    std::cout << "   └─ Bank-PIM: Cross-bank access (40ns latency)\n";
    std::cout << "   └─ Chip-PIM: Cross-chip routing (60ns latency)\n";
    std::cout << "   └─ CPU: Through memory controller + system bus (100ns latency)\n\n";

    std::cout << GREEN << "3. Parallelism vs Locality Trade-off:" << NC << "\n";
    std::cout << "   └─ Subarray-PIM: 64 parallel units, but limited local memory (512KB each)\n";
    std::cout << "   └─ Bank-PIM: 16 parallel units, larger local memory (2MB each)\n";
    std::cout << "   └─ CPU: 1 unit, full memory access but high data movement cost\n\n";

    std::cout << GREEN << "4. Energy Efficiency Hierarchy:" << NC << "\n";
    std::cout << "   └─ Subarray: 1 pJ/byte (just sense amp)\n";
    std::cout << "   └─ Bank: 2 pJ/byte (+ bank switching)\n";
    std::cout << "   └─ Chip: 5 pJ/byte (+ on-chip routing)\n";
    std::cout << "   └─ Rank: 10 pJ/byte (+ I/O drivers)\n";
    std::cout << "   └─ CPU: 20 pJ/byte (+ system bus + LLC)\n\n";

    std::cout << GREEN << "5. Best Architecture Depends on Workload:" << NC << "\n";
    std::cout << "   └─ Data fits in subarray? → Subarray-PIM wins\n";
    std::cout << "   └─ Needs bank-level data? → Bank-PIM wins\n";
    std::cout << "   └─ Random access patterns? → Coarser PIM or CPU may be better\n\n";

    std::cout << MAGENTA << BOLD << "Bottom Line:" << NC << "\n";
    std::cout << "  PIM's advantage comes from REDUCING DATA MOVEMENT, not faster compute!\n";
    std::cout << "  Closer to memory = lower latency + lower energy + higher effective bandwidth\n\n";
}

void visualizeDataPath() {
    std::cout << CYAN << BOLD << "\n╔═══════════════════════════════════════════════════════════╗" << NC << "\n";
    std::cout << CYAN << BOLD << "║              Data Path Visualization                       ║" << NC << "\n";
    std::cout << CYAN << BOLD << "╚═══════════════════════════════════════════════════════════╝" << NC << "\n\n";

    std::cout << "CPU Access Path (longest):\n";
    std::cout << "  " << RED << "CPU Core" << NC << " → LLC → Memory Controller → Rank → Chip → Bank Group → Bank → Subarray\n";
    std::cout << "  └─ Latency: ~100ns, Energy: 20 pJ/byte\n\n";

    std::cout << "MC-PIM Access Path:\n";
    std::cout << "  " << BLUE << "MC Compute" << NC << " → Rank → Chip → Bank Group → Bank → Subarray\n";
    std::cout << "  └─ Latency: ~100ns, Energy: 15 pJ/byte (no system bus)\n\n";

    std::cout << "Rank-PIM Access Path:\n";
    std::cout << "  " << GREEN << "Rank Compute" << NC << " → Chip → Bank Group → Bank → Subarray\n";
    std::cout << "  └─ Latency: ~80ns, Energy: 10 pJ/byte (no I/O to MC)\n\n";

    std::cout << "Chip-PIM Access Path:\n";
    std::cout << "  " << YELLOW << "Chip Compute" << NC << " → Bank Group → Bank → Subarray\n";
    std::cout << "  └─ Latency: ~60ns, Energy: 5 pJ/byte (on-chip only)\n\n";

    std::cout << "Bank-PIM Access Path:\n";
    std::cout << "  " << BLUE << "Bank Compute" << NC << " → Subarray (within bank)\n";
    std::cout << "  └─ Latency: ~40ns, Energy: 2 pJ/byte (bank-local)\n\n";

    std::cout << "Subarray-PIM Access Path (shortest):\n";
    std::cout << "  " << CYAN << "Subarray Compute" << NC << " → Local Subarray Data\n";
    std::cout << "  └─ Latency: ~26ns, Energy: 1 pJ/byte (sense amp only)\n\n";

    std::cout << YELLOW << "Visualization:" << NC << "\n";
    std::cout << "  Data Movement Distance:\n";
    std::cout << "  " << CYAN << "Subarray ■" << NC << "\n";
    std::cout << "  " << BLUE << "Bank     ■■" << NC << "\n";
    std::cout << "  " << YELLOW << "Chip     ■■■" << NC << "\n";
    std::cout << "  " << GREEN << "Rank     ■■■■" << NC << "\n";
    std::cout << "  " << BLUE << "MC       ■■■■■" << NC << "\n";
    std::cout << "  " << RED << "CPU      ■■■■■■" << NC << "\n\n";
}

//=============================================================================
// Main
//=============================================================================
int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << BOLD << CYAN << "╔═══════════════════════════════════════════════════════════╗" << NC << "\n";
    std::cout << BOLD << CYAN << "║      PIM Granularity Deep Dive: DDR4 Architecture         ║" << NC << "\n";
    std::cout << BOLD << CYAN << "║      Subarray vs Bank vs Chip vs Rank vs MC vs CPU        ║" << NC << "\n";
    std::cout << BOLD << CYAN << "╚═══════════════════════════════════════════════════════════╝" << NC << "\n";

    printArchitectureComparison();
    visualizeDataPath();
    runWorkloadComparison();
    analyzeResults();

    std::cout << BOLD << GREEN << "✓ TEST COMPLETE\n" << NC;
    std::cout << "This test demonstrates that PIM's advantage comes from data locality,\n";
    std::cout << "not from having faster compute units!\n\n";

    return 0;
}
