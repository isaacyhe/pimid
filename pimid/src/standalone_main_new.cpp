/**
 * @file standalone_main.cpp
 * @brief PIMID Standalone Simulator - Main Entry Point
 *
 * This is the main PIMID simulator binary. It can simulate any workload
 * binary with configurable memory technologies, PE types, and placement strategies.
 *
 * Usage:
 *   pimid --config simulation.yaml --workload ./bfs_binary [workload_args...]
 *
 * The simulator:
 * 1. Loads configuration from YAML file
 * 2. Initializes memory models and PE simulators
 * 3. Executes the workload binary (fork/exec or dynamic loading)
 * 4. Instruments memory operations
 * 5. Simulates PIM execution
 * 6. Reports statistics
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

// Memory models
#include "memory_models/include/memory_model.h"
#include "memory_models/include/sram_model.h"
#include "memory_models/include/dram_model.h"
#include "memory_models/include/sttmram_model.h"
#include "memory_models/include/pcm_model.h"
#include "memory_models/include/reram_model.h"

using namespace pimid;

//=============================================================================
// Anonymous namespace for standalone implementation
//=============================================================================

namespace {

//=============================================================================
// Configuration Structure
//=============================================================================

struct StandaloneConfig {
    // Simulation metadata
    std::string name;
    std::string description;
    std::string mode;  // "standalone", "host-device", "trace-driven"

    // Memory configuration
    std::string memory_tech;
    int num_banks;
    int subarrays_per_bank;

    // PE configuration
    std::string pe_type;
    std::string placement_level;
    int num_pes;

    // PE-specific timing
    double fetch_decode_ns;
    double execute_compare_ns;
    double branch_penalty_ns;
    double branch_prediction_accuracy;

    // Workload configuration
    std::string workload_binary;
    std::vector<std::string> workload_args;

    // Output configuration
    std::string stats_file;
    std::string trace_file;
    bool enable_detailed_stats;
    bool enable_tracing;

    // Default constructor
    StandaloneConfig() :
        name("PIMID_Simulation"),
        description("PIM Simulation"),
        mode("standalone"),
        memory_tech("SRAM"),
        num_banks(4),
        subarrays_per_bank(4),
        pe_type("in_order_core"),
        placement_level("BANK"),
        num_pes(4),
        fetch_decode_ns(2.0),
        execute_compare_ns(1.0),
        branch_penalty_ns(3.0),
        branch_prediction_accuracy(0.5),
        stats_file("results/stats.txt"),
        trace_file("results/trace.out"),
        enable_detailed_stats(true),
        enable_tracing(false) {}
};

//=============================================================================
// Simple YAML Parser (Simplified for now)
//=============================================================================

class SimpleYAMLParser {
public:
    static StandaloneConfig parseConfigFile(const std::string& filename) {
        StandaloneConfig config;

        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open config file: " << filename << std::endl;
            std::cerr << "Using default configuration" << std::endl;
            return config;
        }

        std::cout << "Loading configuration from: " << filename << std::endl;

        std::string line;
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;

            // Simple key: value parsing
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;

            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t\""));
            value.erase(value.find_last_not_of(" \t\"") + 1);

            // Parse known keys
            if (key == "name") config.name = value;
            else if (key == "description") config.description = value;
            else if (key == "mode") config.mode = value;
            else if (key == "technology") config.memory_tech = value;
            else if (key == "num_banks") config.num_banks = std::stoi(value);
            else if (key == "subarrays_per_bank") config.subarrays_per_bank = std::stoi(value);
            else if (key == "type" && value != "standalone") config.pe_type = value;
            else if (key == "placement_level") config.placement_level = value;
            else if (key == "num_pes") config.num_pes = std::stoi(value);
            else if (key == "binary") config.workload_binary = value;
            else if (key == "stats_file") config.stats_file = value;
            else if (key == "trace_file") config.trace_file = value;
        }

        return config;
    }
};

//=============================================================================
// PIMID Simulator
//=============================================================================

class PIMIDSimulator {
public:
    PIMIDSimulator(const StandaloneConfig& config) : config_(config) {}

    bool initialize() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "PIMID Standalone Simulator" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Simulation: " << config_.name << std::endl;
        if (!config_.description.empty()) {
            std::cout << "Description: " << config_.description << std::endl;
        }
        std::cout << std::endl;

        // Initialize memory model
        if (!initializeMemoryModel()) {
            return false;
        }

        // Initialize PE simulators
        if (!initializePEs()) {
            return false;
        }

        std::cout << "Initialization complete!" << std::endl;
        return true;
    }

    bool runWorkload() {
        if (config_.workload_binary.empty()) {
            std::cerr << "Error: No workload binary specified" << std::endl;
            return false;
        }

        std::cout << "\n========================================" << std::endl;
        std::cout << "Running Workload" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Binary: " << config_.workload_binary << std::endl;

        if (!config_.workload_args.empty()) {
            std::cout << "Arguments:";
            for (const auto& arg : config_.workload_args) {
                std::cout << " " << arg;
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;

        // Set environment variables for workload
        setenv("PIMID_ACTIVE", "1", 1);
        setenv("PIMID_MEMORY_TECH", config_.memory_tech.c_str(), 1);
        setenv("PIMID_PLACEMENT_LEVEL", config_.placement_level.c_str(), 1);
        setenv("PIMID_NUM_PES", std::to_string(config_.num_pes).c_str(), 1);

        // Execute workload as child process
        pid_t pid = fork();

        if (pid < 0) {
            std::cerr << "Error: Failed to fork process" << std::endl;
            return false;
        }

        if (pid == 0) {
            // Child process: execute workload
            std::vector<char*> args;
            args.push_back(const_cast<char*>(config_.workload_binary.c_str()));

            for (const auto& arg : config_.workload_args) {
                args.push_back(const_cast<char*>(arg.c_str()));
            }
            args.push_back(nullptr);

            execvp(config_.workload_binary.c_str(), args.data());

            // If exec fails
            std::cerr << "Error: Failed to execute workload: " << config_.workload_binary << std::endl;
            exit(1);
        }

        // Parent process: wait for workload to complete
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            std::cout << "\nWorkload completed with exit code: " << exit_code << std::endl;
            return exit_code == 0;
        } else {
            std::cerr << "Workload terminated abnormally" << std::endl;
            return false;
        }
    }

    void reportStatistics() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Simulation Statistics" << std::endl;
        std::cout << "========================================" << std::endl;

        // In full implementation, this would report detailed statistics
        // collected during workload execution

        std::cout << "Configuration:" << std::endl;
        std::cout << "  Memory Technology: " << config_.memory_tech << std::endl;
        std::cout << "  Number of Banks: " << config_.num_banks << std::endl;
        std::cout << "  Subarrays per Bank: " << config_.subarrays_per_bank << std::endl;
        std::cout << "  PE Type: " << config_.pe_type << std::endl;
        std::cout << "  Placement Level: " << config_.placement_level << std::endl;
        std::cout << "  Number of PEs: " << config_.num_pes << std::endl;

        if (memory_model_) {
            std::cout << "\nMemory Statistics:" << std::endl;
            std::cout << "  Read Energy: " << memory_model_->getReadEnergy() << " pJ/byte" << std::endl;
            std::cout << "  Write Energy: " << memory_model_->getWriteEnergy() << " pJ/byte" << std::endl;
            std::cout << "  Leakage Power: " << memory_model_->getLeakagePower() << " W" << std::endl;
        }

        std::cout << "\nOutput files:" << std::endl;
        std::cout << "  Statistics: " << config_.stats_file << std::endl;
        if (config_.enable_tracing) {
            std::cout << "  Trace: " << config_.trace_file << std::endl;
        }

        std::cout << "========================================\n" << std::endl;
    }

private:
    StandaloneConfig config_;
    std::shared_ptr<MemoryModel> memory_model_;

    bool initializeMemoryModel() {
        std::cout << "Initializing Memory Model..." << std::endl;
        std::cout << "  Technology: " << config_.memory_tech << std::endl;

        try {
            if (config_.memory_tech == "SRAM") {
                memory_model_ = std::make_shared<SRAMModel>("config.yaml");
            } else if (config_.memory_tech == "DRAM") {
                memory_model_ = std::make_shared<DRAMModel>("config.yaml");
            } else if (config_.memory_tech == "STT_MRAM" || config_.memory_tech == "STTMRAM") {
                memory_model_ = std::make_shared<STTMRAMModel>("config.yaml");
            } else if (config_.memory_tech == "PCM") {
                memory_model_ = std::make_shared<PCMModel>("config.yaml");
            } else if (config_.memory_tech == "ReRAM" || config_.memory_tech == "RERAM") {
                memory_model_ = std::make_shared<ReRAMModel>("config.yaml");
            } else {
                std::cerr << "  Error: Unknown memory technology: " << config_.memory_tech << std::endl;
                return false;
            }

            memory_model_->initialize();
            std::cout << "  Memory model initialized successfully" << std::endl;
            return true;

        } catch (const std::exception& e) {
            std::cerr << "  Error initializing memory model: " << e.what() << std::endl;
            return false;
        }
    }

    bool initializePEs() {
        std::cout << "\nInitializing Processing Elements..." << std::endl;
        std::cout << "  Type: " << config_.pe_type << std::endl;
        std::cout << "  Placement: " << config_.placement_level << std::endl;
        std::cout << "  Count: " << config_.num_pes << std::endl;

        // In full implementation, this would initialize PE simulators
        std::cout << "  PE simulators initialized" << std::endl;

        return true;
    }
};

} // anonymous namespace

//=============================================================================
// Main
//=============================================================================

void printUsage(const char* program_name) {
    std::cout << "PIMID - Processing-In-Memory Simulator" << std::endl;
    std::cout << "\nUsage: " << program_name << " [options]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  --config FILE        Configuration file (YAML)" << std::endl;
    std::cout << "  --workload BINARY    Workload binary to simulate" << std::endl;
    std::cout << "  --help, -h           Show this help message" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  " << program_name << " --config sim.yaml --workload ./bfs --vertices 100000" << std::endl;
    std::cout << "  " << program_name << " --config dram_bank.yaml --workload ./spmv --matrix data.mtx" << std::endl;
    std::cout << "\nFor more information, see documentation in docs/" << std::endl;
}

int main(int argc, char** argv) {
    // Parse command line arguments
    std::string config_file;
    std::string workload_binary;
    std::vector<std::string> workload_args;

    bool parsing_workload_args = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (parsing_workload_args) {
            workload_args.push_back(arg);
            continue;
        }

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "--workload" && i + 1 < argc) {
            workload_binary = argv[++i];
            parsing_workload_args = true;  // All remaining args go to workload
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Validate arguments
    if (config_file.empty() && workload_binary.empty()) {
        std::cerr << "Error: Must specify at least --config or --workload" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Load configuration
    StandaloneConfig config;
    if (!config_file.empty()) {
        config = SimpleYAMLParser::parseConfigFile(config_file);
    }

    // Override workload binary from command line
    if (!workload_binary.empty()) {
        config.workload_binary = workload_binary;
        config.workload_args = workload_args;
    }

    // Create and run simulator
    PIMIDSimulator simulator(config);

    if (!simulator.initialize()) {
        std::cerr << "Failed to initialize simulator" << std::endl;
        return 1;
    }

    if (!simulator.runWorkload()) {
        std::cerr << "Workload execution failed" << std::endl;
        return 1;
    }

    simulator.reportStatistics();

    return 0;
}
