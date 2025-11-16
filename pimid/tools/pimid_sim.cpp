/**
 * @file pimid_sim.cpp
 * @brief Command-Line PIM Simulator with Unified Config File Support
 *
 * This tool allows users to run PIM simulations from the command line
 * using a single unified YAML configuration file, without needing to
 * modify C++ code or recompile.
 *
 * Usage:
 *   pimid_sim <config_file.yaml> [options]
 *
 * Example:
 *   pimid_sim configs/example_pim_config.yaml --output results.csv
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <algorithm>

// PIM Simulator components
#include "memory_models/include/pim_request_payload.h"
#include "memory_models/include/pim_bandwidth_tracker.h"
#include "memory/dram_architecture_v2.h"

using namespace pimid;
using namespace pimid::memory;

// ============================================================================
// Configuration Structures (matching YAML schema)
// ============================================================================

struct SimulationConfig {
    std::string name;
    std::string description;
    std::string output_format;  // csv, json, text, all
    std::string output_directory;
    bool detailed_stats;
    bool compare_configs;
};

struct DRAMConfig {
    std::string type;  // DDR4-2400, DDR4-3200, HBM2, HBM3
    int channels;
    int ranks_per_channel;
    int bank_groups;
    int banks_per_group;
    int subarrays_per_bank;
};

struct PIMConfig {
    std::string granularity;  // CPU, MEMORY_CONTROLLER, RANK, CHIP, BANK_GROUP, BANK, SUBARRAY
    int num_pes;
    double compute_gflops;
    bool compute_per_pe;
};

struct WorkloadConfig {
    uint64_t total_data_bytes;
    uint64_t total_compute_ops;
    double local_data_fraction;
    std::string access_pattern;  // sequential, random, custom
    int cache_line_size;
};

struct WorkloadPattern {
    std::string name;
    std::string description;
    uint64_t total_data_bytes;
    double local_data_fraction;
};

struct ComparisonConfig {
    bool enabled;
    std::vector<std::string> granularities;
    std::vector<WorkloadPattern> workload_patterns;
};

struct OutputConfig {
    std::string prefix;
    std::vector<std::string> csv_columns;
    bool generate_plots;
    std::string plot_type;  // png, pdf, svg
};

struct UnifiedConfig {
    SimulationConfig simulation;
    DRAMConfig dram;
    PIMConfig pim;
    WorkloadConfig workload;
    ComparisonConfig comparison;
    OutputConfig output;
};

// ============================================================================
// Simulation Result Structure
// ============================================================================

struct SimulationResult {
    std::string config_name;
    PIMGranularity granularity;
    int num_pes;
    uint64_t local_data_bytes;
    uint64_t remote_data_bytes;
    double local_fraction;

    // Time breakdown (microseconds)
    double compute_time_us;
    double local_access_time_us;
    double remote_access_time_us;
    double network_time_us;
    double total_time_us;

    // Performance metrics
    double speedup;
    std::string bottleneck;  // "Compute", "LocalBW", "RemoteBW", "Network"

    // Bandwidth utilization
    double local_bw_GBs;
    double remote_bw_GBs;
    double effective_bw_GBs;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Parse human-readable size string (e.g., "1GB", "512MB", "64KB")
 */
uint64_t parseSize(const std::string& size_str) {
    std::string str = size_str;
    // Remove spaces
    str.erase(std::remove(str.begin(), str.end(), ' '), str.end());

    // Find numeric part
    size_t pos = 0;
    double value = std::stod(str, &pos);

    // Find suffix
    std::string suffix = str.substr(pos);
    std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::toupper);

    uint64_t multiplier = 1;
    if (suffix == "KB") multiplier = 1024ULL;
    else if (suffix == "MB") multiplier = 1024ULL * 1024;
    else if (suffix == "GB") multiplier = 1024ULL * 1024 * 1024;
    else if (suffix == "TB") multiplier = 1024ULL * 1024 * 1024 * 1024;
    else if (suffix == "KFLOP") multiplier = 1000ULL;
    else if (suffix == "MFLOP") multiplier = 1000ULL * 1000;
    else if (suffix == "GFLOP") multiplier = 1000ULL * 1000 * 1000;
    else if (suffix == "TFLOP") multiplier = 1000ULL * 1000 * 1000 * 1000;

    return static_cast<uint64_t>(value * multiplier);
}

/**
 * @brief Format bytes to human-readable string
 */
std::string formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double value = static_cast<double>(bytes);

    while (value >= 1024.0 && unit_idx < 4) {
        value /= 1024.0;
        unit_idx++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value << " " << units[unit_idx];
    return oss.str();
}

/**
 * @brief Convert granularity string to enum
 */
PIMGranularity stringToGranularity(const std::string& str) {
    if (str == "CPU") return PIMGranularity::CPU;
    if (str == "MEMORY_CONTROLLER") return PIMGranularity::MEMORY_CONTROLLER;
    if (str == "RANK") return PIMGranularity::RANK;
    if (str == "CHIP") return PIMGranularity::CHIP;
    if (str == "BANK_GROUP") return PIMGranularity::BANK_GROUP;
    if (str == "BANK") return PIMGranularity::BANK;
    if (str == "SUBARRAY") return PIMGranularity::SUBARRAY;
    return PIMGranularity::CPU;
}

/**
 * @brief Convert granularity enum to string
 */
const char* granularityToString(PIMGranularity granularity) {
    switch (granularity) {
        case PIMGranularity::CPU: return "CPU";
        case PIMGranularity::MEMORY_CONTROLLER: return "MemoryController";
        case PIMGranularity::RANK: return "Rank";
        case PIMGranularity::CHIP: return "Chip";
        case PIMGranularity::BANK_GROUP: return "BankGroup";
        case PIMGranularity::BANK: return "Bank";
        case PIMGranularity::SUBARRAY: return "Subarray";
        default: return "Unknown";
    }
}

/**
 * @brief Get access latency overhead for each PIM granularity
 */
double getAccessLatencyOverhead(
    PIMGranularity granularity,
    std::shared_ptr<DRAMArchitectureV2> dram_arch) {

    // Base DRAM access latency (tRCD + tCAS)
    double base_dram_latency_ns = dram_arch->timing.tRCD_ns + dram_arch->timing.tCAS_ns;

    switch (granularity) {
        case PIMGranularity::CPU:
            // Host CPU: Cache hierarchy + coherence + OS overhead + distance from MC
            return base_dram_latency_ns + 150.0;  // ~180ns total

        case PIMGranularity::MEMORY_CONTROLLER:
            // MC-PIM: At memory controller, no cache/coherence overhead, but MC logic delay
            return base_dram_latency_ns + 40.0;   // ~70ns total

        case PIMGranularity::RANK:
            // Rank-PIM: Even closer to DRAM, minimal MC overhead
            return base_dram_latency_ns + 15.0;   // ~42ns total

        case PIMGranularity::CHIP:
            // Chip-PIM: Close to banks
            return base_dram_latency_ns + 7.0;    // ~34ns total

        case PIMGranularity::BANK_GROUP:
            // BG-PIM: Very close to data
            return base_dram_latency_ns + 3.0;    // ~30ns total

        case PIMGranularity::BANK:
        case PIMGranularity::SUBARRAY:
            // Bank/Subarray-PIM: Directly at the data, minimal overhead
            return base_dram_latency_ns;          // ~27ns total

        default:
            return base_dram_latency_ns;
    }
}

/**
 * @brief Simple YAML-like config parser (minimal implementation)
 * For production, use yaml-cpp library
 */
class SimpleConfigParser {
public:
    std::map<std::string, std::string> values;

    bool parse(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open config file: " << filename << std::endl;
            return false;
        }

        std::string line;
        std::string current_section;

        while (std::getline(file, line)) {
            // Remove comments
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }

            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);

            if (line.empty()) continue;

            // Check for section header
            if (line.back() == ':' && line.find(' ') == std::string::npos) {
                current_section = line.substr(0, line.length() - 1);
                continue;
            }

            // Parse key-value pair
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);

                // Trim key and value
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                // Store with section prefix
                std::string full_key = current_section.empty() ? key : current_section + "." + key;
                values[full_key] = value;
            }
        }

        return true;
    }

    std::string getString(const std::string& key, const std::string& default_value = "") {
        auto it = values.find(key);
        return (it != values.end()) ? it->second : default_value;
    }

    int getInt(const std::string& key, int default_value = 0) {
        auto it = values.find(key);
        return (it != values.end()) ? std::stoi(it->second) : default_value;
    }

    double getDouble(const std::string& key, double default_value = 0.0) {
        auto it = values.find(key);
        return (it != values.end()) ? std::stod(it->second) : default_value;
    }

    bool getBool(const std::string& key, bool default_value = false) {
        auto it = values.find(key);
        if (it == values.end()) return default_value;
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return value == "true" || value == "yes" || value == "1";
    }
};

// ============================================================================
// Simulation Engine
// ============================================================================

class PIMSimulator {
public:
    PIMSimulator(const UnifiedConfig& config)
        : config_(config) {
        setupDRAMArchitecture();
    }

    /**
     * @brief Run a single simulation configuration
     */
    SimulationResult runSimulation(
        PIMGranularity granularity,
        const WorkloadConfig& workload,
        const std::string& config_name) {

        SimulationResult result;
        result.config_name = config_name;
        result.granularity = granularity;
        result.num_pes = config_.pim.num_pes;

        // Calculate local vs remote data based on granularity and data distribution
        result.local_data_bytes = static_cast<uint64_t>(
            workload.total_data_bytes * workload.local_data_fraction);
        result.remote_data_bytes = workload.total_data_bytes - result.local_data_bytes;
        result.local_fraction = workload.local_data_fraction;

        // Get bandwidth limits for this granularity
        double local_bw_GBs = getBandwidthForGranularity(granularity);
        double remote_bw_GBs = getBandwidthForGranularity(PIMGranularity::RANK);  // Remote via rank interface

        result.local_bw_GBs = local_bw_GBs;
        result.remote_bw_GBs = remote_bw_GBs;

        // Calculate compute time
        double total_gflops = config_.pim.compute_per_pe
            ? config_.pim.compute_gflops * config_.pim.num_pes
            : config_.pim.compute_gflops;

        double compute_ops = static_cast<double>(workload.total_compute_ops);
        result.compute_time_us = (compute_ops / total_gflops) / 1e3;  // GFLOP/GFLOPS = seconds, /1e3 = us

        // Calculate local data access time (bandwidth + latency)
        double local_bw_time_us = (result.local_data_bytes / local_bw_GBs) / 1e3;  // GB/(GB/s) = s, /1e3 = us

        // Calculate access latency overhead
        double access_latency_ns = getAccessLatencyOverhead(granularity, dram_arch_);
        uint64_t num_local_accesses = (result.local_data_bytes + workload.cache_line_size - 1) / workload.cache_line_size;
        double local_latency_time_us = (num_local_accesses * access_latency_ns) / 1e3;

        result.local_access_time_us = std::max(local_latency_time_us, local_bw_time_us);

        // Calculate remote data access time (higher latency + network overhead)
        if (result.remote_data_bytes > 0) {
            double remote_bw_time_us = (result.remote_data_bytes / remote_bw_GBs) / 1e3;
            uint64_t num_remote_accesses = (result.remote_data_bytes + workload.cache_line_size - 1) / workload.cache_line_size;

            // Remote access has additional network latency (100ns per hop estimate)
            double remote_latency_ns = access_latency_ns + 100.0;
            double remote_latency_time_us = (num_remote_accesses * remote_latency_ns) / 1e3;

            result.remote_access_time_us = std::max(remote_latency_time_us, remote_bw_time_us);

            // Network time for internal DRAM transfers
            result.network_time_us = remote_latency_time_us * 0.1;  // ~10% network overhead
        } else {
            result.remote_access_time_us = 0.0;
            result.network_time_us = 0.0;
        }

        // Total time (assuming overlapped compute and data movement for PIM)
        if (granularity == PIMGranularity::CPU) {
            // Host CPU: sequential execution (compute after data load)
            result.total_time_us = result.compute_time_us + result.local_access_time_us + result.remote_access_time_us;
        } else {
            // PIM: overlapped execution, limited by slowest component
            result.total_time_us = std::max({
                result.compute_time_us,
                result.local_access_time_us,
                result.remote_access_time_us
            }) + result.network_time_us;
        }

        // Determine bottleneck
        if (result.compute_time_us >= result.local_access_time_us &&
            result.compute_time_us >= result.remote_access_time_us) {
            result.bottleneck = "Compute";
        } else if (result.local_access_time_us >= result.remote_access_time_us) {
            result.bottleneck = "LocalBW";
        } else {
            result.bottleneck = "RemoteBW";
        }

        // Calculate effective bandwidth
        result.effective_bw_GBs = (workload.total_data_bytes / 1e9) / (result.total_time_us / 1e6);

        return result;
    }

    /**
     * @brief Run comparison across multiple configurations
     */
    std::vector<SimulationResult> runComparison() {
        std::vector<SimulationResult> results;

        if (!config_.comparison.enabled) {
            // Single configuration
            PIMGranularity granularity = stringToGranularity(config_.pim.granularity);
            auto result = runSimulation(granularity, config_.workload, config_.simulation.name);
            results.push_back(result);
            return results;
        }

        // Multiple configurations
        for (const auto& pattern : config_.comparison.workload_patterns) {
            WorkloadConfig workload = config_.workload;
            workload.total_data_bytes = pattern.total_data_bytes;
            workload.local_data_fraction = pattern.local_data_fraction;

            for (const auto& gran_str : config_.comparison.granularities) {
                PIMGranularity granularity = stringToGranularity(gran_str);
                std::string config_name = pattern.name + "-" + gran_str;
                auto result = runSimulation(granularity, workload, config_name);
                results.push_back(result);
            }
        }

        // Calculate speedups (relative to CPU baseline)
        for (size_t i = 0; i < config_.comparison.workload_patterns.size(); i++) {
            // Find CPU baseline for this workload pattern
            double cpu_time = 0.0;
            for (const auto& result : results) {
                if (result.config_name.find(config_.comparison.workload_patterns[i].name) != std::string::npos &&
                    result.granularity == PIMGranularity::CPU) {
                    cpu_time = result.total_time_us;
                    break;
                }
            }

            // Calculate speedups
            if (cpu_time > 0.0) {
                for (auto& result : results) {
                    if (result.config_name.find(config_.comparison.workload_patterns[i].name) != std::string::npos) {
                        result.speedup = cpu_time / result.total_time_us;
                    }
                }
            }
        }

        return results;
    }

private:
    UnifiedConfig config_;
    std::shared_ptr<DRAMArchitectureV2> dram_arch_;

    void setupDRAMArchitecture() {
        // Create DRAM architecture based on config
        dram_arch_ = std::make_shared<DRAMArchitectureV2>(config_.dram.type, "DDR4");

        // Set up based on DRAM type
        if (config_.dram.type.find("DDR4-2400") != std::string::npos) {
            dram_arch_->timing.clock_freq_mhz = 1200;  // DDR4-2400 = 1200 MHz
            dram_arch_->timing.tRCD_ns = 13.32;
            dram_arch_->timing.tCAS_ns = 13.32;
            dram_arch_->timing.tRP_ns = 13.32;
        } else if (config_.dram.type.find("DDR4-3200") != std::string::npos) {
            dram_arch_->timing.clock_freq_mhz = 1600;  // DDR4-3200 = 1600 MHz
            dram_arch_->timing.tRCD_ns = 13.75;
            dram_arch_->timing.tCAS_ns = 13.75;
            dram_arch_->timing.tRP_ns = 13.75;
        } else if (config_.dram.type.find("HBM2") != std::string::npos) {
            dram_arch_->timing.clock_freq_mhz = 1000;  // HBM2 = 1 GHz
            dram_arch_->timing.tRCD_ns = 14.0;
            dram_arch_->timing.tCAS_ns = 14.0;
            dram_arch_->timing.tRP_ns = 14.0;
        }

        // Set organization
        dram_arch_->organization.subarrays_per_bank = config_.dram.subarrays_per_bank;
        dram_arch_->organization.banks_per_bank_group = config_.dram.banks_per_group;
        dram_arch_->organization.bank_groups_per_chip = config_.dram.bank_groups;
        dram_arch_->organization.ranks_per_channel = config_.dram.ranks_per_channel;
    }

    double getBandwidthForGranularity(PIMGranularity granularity) const {
        double freq_GHz = dram_arch_->timing.clock_freq_mhz / 1000.0;

        // Configuration
        int num_channels = config_.dram.channels;

        // Bank I/O width depends on device type (x4, x8, x16)
        // DDR4 x8 device: 8-bit per bank
        // DDR4 x16 device: 16-bit per bank
        // This is the internal per-bank I/O width
        double bank_io_width = 8.0;  // Default: x8 device (8-bit per bank)

        // Rank interface width: typically 64-bit (8 chips × 8-bit each for x8 devices)
        double rank_interface_width = 64.0;

        // Bandwidth Hierarchy (for LOCAL data):
        // - Host/MC: External interface, 64-bit per channel × num_channels
        // - Rank: External rank interface, 64-bit
        // - Chip/BG/Bank: Internal bank I/O, same width (8-bit or 16-bit per bank)
        //
        // Key: Chip/BG/Bank all use the SAME internal bank I/O interface!

        switch (granularity) {
            case PIMGranularity::CPU:
                // Host CPU: Can access all channels in parallel
                // Ranks share the same channel bus (time-multiplexed), only channels add bandwidth
                // Interface BW = 64-bit per channel × num channels
                return (rank_interface_width / 8.0) * num_channels * freq_GHz;

            case PIMGranularity::MEMORY_CONTROLLER:
                // MC-PIM: At memory controller, can access ALL channels in parallel!
                // This is the HIGHEST aggregate interface bandwidth
                // Ranks are time-multiplexed on each channel, only channels add parallel bandwidth
                // Interface BW = 64-bit per channel × num channels
                return (rank_interface_width / 8.0) * num_channels * freq_GHz;

            case PIMGranularity::RANK:
                // Rank-PIM: Uses external rank interface (64-bit)
                // For accessing data across banks within the rank
                return (rank_interface_width / 8.0) * freq_GHz;

            case PIMGranularity::CHIP:
            case PIMGranularity::BANK_GROUP:
            case PIMGranularity::BANK:
                // Chip/BG/Bank-PIM: All use internal bank I/O interface
                // Same bandwidth = bank I/O width (8-bit for x8, 16-bit for x16)
                // These are INSIDE the chip, accessing local data through bank I/O
                // CRITICAL BOTTLENECK: 8-bit per bank serialization!
                return (bank_io_width / 8.0) * freq_GHz;

            case PIMGranularity::SUBARRAY:
                // Subarray-PIM: Can access full row buffer width (256-bit GSA)
                // Internal bandwidth, highest for compute-in-place
                return (256.0 / 8.0) * freq_GHz;

            default:
                return (rank_interface_width / 8.0) * freq_GHz;
        }
    }
};

// ============================================================================
// Output Formatters
// ============================================================================

void outputCSV(const std::vector<SimulationResult>& results, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open output file: " << filename << std::endl;
        return;
    }

    // Header
    file << "config_name,granularity,num_pes,local_data_bytes,remote_data_bytes,local_fraction,"
         << "compute_time_us,local_access_time_us,remote_access_time_us,network_time_us,total_time_us,"
         << "speedup,bottleneck,local_bw_GBs,remote_bw_GBs,effective_bw_GBs\n";

    // Data rows
    for (const auto& result : results) {
        file << result.config_name << ","
             << granularityToString(result.granularity) << ","
             << result.num_pes << ","
             << result.local_data_bytes << ","
             << result.remote_data_bytes << ","
             << std::fixed << std::setprecision(3) << result.local_fraction << ","
             << std::setprecision(2) << result.compute_time_us << ","
             << result.local_access_time_us << ","
             << result.remote_access_time_us << ","
             << result.network_time_us << ","
             << result.total_time_us << ","
             << std::setprecision(3) << result.speedup << ","
             << result.bottleneck << ","
             << std::setprecision(2) << result.local_bw_GBs << ","
             << result.remote_bw_GBs << ","
             << result.effective_bw_GBs << "\n";
    }

    file.close();
    std::cout << "CSV output written to: " << filename << std::endl;
}

void outputText(const std::vector<SimulationResult>& results) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "PIM Simulation Results\n";
    std::cout << "========================================\n\n";

    for (const auto& result : results) {
        std::cout << "Configuration: " << result.config_name << "\n";
        std::cout << "  Granularity:   " << granularityToString(result.granularity) << "\n";
        std::cout << "  Num PEs:       " << result.num_pes << "\n";
        std::cout << "  Local Data:    " << formatBytes(result.local_data_bytes)
                  << " (" << std::fixed << std::setprecision(1) << (result.local_fraction * 100) << "%)\n";
        std::cout << "  Remote Data:   " << formatBytes(result.remote_data_bytes) << "\n";
        std::cout << "\n";
        std::cout << "  Time Breakdown:\n";
        std::cout << "    Compute:        " << std::setprecision(2) << result.compute_time_us << " us\n";
        std::cout << "    Local Access:   " << result.local_access_time_us << " us\n";
        std::cout << "    Remote Access:  " << result.remote_access_time_us << " us\n";
        std::cout << "    Network:        " << result.network_time_us << " us\n";
        std::cout << "    TOTAL:          " << result.total_time_us << " us\n";
        std::cout << "\n";
        std::cout << "  Performance:\n";
        std::cout << "    Speedup:        " << std::setprecision(2) << result.speedup << "x\n";
        std::cout << "    Bottleneck:     " << result.bottleneck << "\n";
        std::cout << "    Effective BW:   " << result.effective_bw_GBs << " GB/s\n";
        std::cout << "\n";
        std::cout << "----------------------------------------\n\n";
    }
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file.yaml> [--output <file.csv>]\n";
        std::cerr << "\n";
        std::cerr << "Example:\n";
        std::cerr << "  " << argv[0] << " configs/example_pim_config.yaml\n";
        std::cerr << "  " << argv[0] << " configs/example_pim_config.yaml --output results.csv\n";
        return 1;
    }

    std::string config_file = argv[1];
    std::string output_file;

    // Parse command-line arguments
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--output" && i + 1 < argc) {
            output_file = argv[++i];
        }
    }

    // Parse configuration file
    std::cout << "Loading configuration from: " << config_file << std::endl;
    SimpleConfigParser parser;
    if (!parser.parse(config_file)) {
        return 1;
    }

    // Build unified config
    UnifiedConfig config;

    // Simulation config
    config.simulation.name = parser.getString("simulation.name", "PIM Simulation");
    config.simulation.description = parser.getString("simulation.description", "");
    config.simulation.output_format = parser.getString("simulation.output_format", "csv");
    config.simulation.output_directory = parser.getString("simulation.output_directory", "./results");
    config.simulation.detailed_stats = parser.getBool("simulation.detailed_stats", true);
    config.simulation.compare_configs = parser.getBool("simulation.compare_configs", true);

    // DRAM config
    config.dram.type = parser.getString("dram.type", "DDR4-2400");
    config.dram.channels = parser.getInt("dram.channels", 1);
    config.dram.ranks_per_channel = parser.getInt("dram.ranks_per_channel", 1);
    config.dram.bank_groups = parser.getInt("dram.bank_groups", 4);
    config.dram.banks_per_group = parser.getInt("dram.banks_per_group", 4);
    config.dram.subarrays_per_bank = parser.getInt("dram.subarrays_per_bank", 16);

    // PIM config
    config.pim.granularity = parser.getString("pim.granularity", "BANK");
    config.pim.num_pes = parser.getInt("pim.num_pes", 64);
    config.pim.compute_gflops = parser.getDouble("pim.compute_gflops", 128.0);
    config.pim.compute_per_pe = parser.getBool("pim.compute_per_pe", false);

    // Workload config
    std::string data_bytes_str = parser.getString("workload.total_data_bytes", "1GB");
    config.workload.total_data_bytes = parseSize(data_bytes_str);

    std::string compute_ops_str = parser.getString("workload.total_compute_ops", "128GFLOP");
    config.workload.total_compute_ops = parseSize(compute_ops_str);

    config.workload.local_data_fraction = parser.getDouble("workload.local_data_fraction", 0.5);
    config.workload.access_pattern = parser.getString("workload.access_pattern", "random");
    config.workload.cache_line_size = parser.getInt("workload.cache_line_size", 64);

    // Comparison config
    config.comparison.enabled = parser.getBool("comparison.enabled", false);

    // For simplicity, hardcode comparison patterns (production should parse from YAML)
    if (config.comparison.enabled) {
        config.comparison.granularities = {"CPU", "MEMORY_CONTROLLER", "RANK", "CHIP", "BANK_GROUP", "BANK"};

        WorkloadPattern local_pattern;
        local_pattern.name = "LOCAL";
        local_pattern.description = "All data fits locally";
        local_pattern.total_data_bytes = parseSize("64MB");
        local_pattern.local_data_fraction = 1.0;
        config.comparison.workload_patterns.push_back(local_pattern);

        WorkloadPattern global_pattern;
        global_pattern.name = "GLOBAL";
        global_pattern.description = "Data spread across all banks";
        global_pattern.total_data_bytes = parseSize("1GB");
        global_pattern.local_data_fraction = 0.0625;  // 1/16 banks
        config.comparison.workload_patterns.push_back(global_pattern);

        WorkloadPattern partial_pattern;
        partial_pattern.name = "PARTIAL";
        partial_pattern.description = "50% local, 50% remote";
        partial_pattern.total_data_bytes = parseSize("512MB");
        partial_pattern.local_data_fraction = 0.5;
        config.comparison.workload_patterns.push_back(partial_pattern);
    }

    // Output config
    config.output.prefix = parser.getString("output.prefix", "pim_sim");
    config.output.generate_plots = parser.getBool("output.generate_plots", false);
    config.output.plot_type = parser.getString("output.plot_type", "png");

    // Run simulation
    std::cout << "Running PIM simulation...\n";
    PIMSimulator simulator(config);
    std::vector<SimulationResult> results = simulator.runComparison();

    // Output results
    outputText(results);

    // Write CSV if requested
    if (!output_file.empty()) {
        outputCSV(results, output_file);
    } else if (config.simulation.output_format.find("csv") != std::string::npos) {
        std::string default_csv = config.output.prefix + "_results.csv";
        outputCSV(results, default_csv);
    }

    std::cout << "\nSimulation completed successfully!\n";
    return 0;
}
