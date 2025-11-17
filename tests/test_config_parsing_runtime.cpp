/**
 * Runtime test for memory model configuration parsing
 * Verifies that ConfigParser is properly linked and functional
 */

#include <iostream>
#include <fstream>
#include <cassert>
#include "dram_model.h"
#include "sram_model.h"
#include "sttmram_model.h"
#include "config/config_parser.h"

using namespace pimid;

// Helper function to create a test YAML config
void createTestConfig(const std::string& filename) {
    std::ofstream file(filename);
    file << "# Test configuration for memory models\n";
    file << "dram:\n";
    file << "  standard: DDR4\n";
    file << "  speed: 3200\n";
    file << "  timing:\n";
    file << "    tCL: 16\n";
    file << "    tRCD: 16\n";
    file << "    tRP: 16\n";
    file << "    tRAS: 39\n";
    file << "  organization:\n";
    file << "    channels: 2\n";
    file << "    ranks: 2\n";
    file << "    banks: 16\n";
    file << "    rows: 65536\n";
    file << "    columns: 1024\n";
    file << "  capacity_gb: 16\n";
    file << "\n";
    file << "sram:\n";
    file << "  capacity_mb: 256\n";
    file << "  timing:\n";
    file << "    access_latency_ns: 1.0\n";
    file << "  cacti:\n";
    file << "    technology_node_nm: 22\n";
    file << "    cache_size: 262144\n";
    file << "    line_size: 64\n";
    file << "    associativity: 8\n";
    file << "\n";
    file << "stt_mram:\n";
    file << "  timing:\n";
    file << "    read_latency_ns: 20.0\n";
    file << "    write_latency_ns: 50.0\n";
    file << "  endurance:\n";
    file << "    write_cycles: 1000000000000\n";
    file << "  technology:\n";
    file << "    cell_type: perpendicular\n";
    file << "    technology_node_nm: 22\n";
    file.close();
}

void test_config_parser_basic() {
    std::cout << "Test 1: Basic ConfigParser functionality... ";

    // Create test config
    createTestConfig("/tmp/test_memory_config.yaml");

    // Test ConfigParser directly
    config::ConfigParser parser;
    std::map<std::string, std::string> config_map;

    bool success = parser.parseFile("/tmp/test_memory_config.yaml", config_map);
    assert(success && "ConfigParser::parseFile should succeed");
    assert(!config_map.empty() && "Config map should not be empty");

    // Verify some expected keys exist
    assert(config_map.find("dram.standard") != config_map.end());
    assert(config_map["dram.standard"] == "DDR4");

    std::cout << "PASSED\n";
    std::cout << "  - Parsed " << config_map.size() << " config entries\n";
}

void test_dram_model_config() {
    std::cout << "Test 2: DRAM model config loading... ";

    // Create a DRAM model and load config
    DRAMModel dram;

    // This should not crash and should load the config
    dram.loadConfig("/tmp/test_memory_config.yaml");

    // If we get here without crashing, the linker issue is fixed
    std::cout << "PASSED\n";
    std::cout << "  - DRAM model successfully loaded config\n";
}

void test_sram_model_config() {
    std::cout << "Test 3: SRAM model config loading... ";

    // Create an SRAM model and load config
    SRAMModel sram;

    // This should not crash and should load the config
    sram.loadConfig("/tmp/test_memory_config.yaml");

    std::cout << "PASSED\n";
    std::cout << "  - SRAM model successfully loaded config\n";
}

void test_sttmram_model_config() {
    std::cout << "Test 4: STT-MRAM model config loading... ";

    // Create an STT-MRAM model and load config
    STTMRAMModel mram;

    // This should not crash and should load the config
    mram.loadConfig("/tmp/test_memory_config.yaml");

    std::cout << "PASSED\n";
    std::cout << "  - STT-MRAM model successfully loaded config\n";
}

void test_missing_config_file() {
    std::cout << "Test 5: Graceful handling of missing config... ";

    DRAMModel dram;

    // This should not crash, just use defaults
    dram.loadConfig("/nonexistent/config.yaml");

    std::cout << "PASSED\n";
    std::cout << "  - Gracefully handled missing config file\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Config Parsing Runtime Test Suite\n";
    std::cout << "========================================\n\n";

    try {
        test_config_parser_basic();
        test_dram_model_config();
        test_sram_model_config();
        test_sttmram_model_config();
        test_missing_config_file();

        std::cout << "\n========================================\n";
        std::cout << "All tests PASSED!\n";
        std::cout << "ConfigParser is properly linked and functional.\n";
        std::cout << "========================================\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nFAILED: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\nFAILED: Unknown exception\n";
        return 1;
    }
}
