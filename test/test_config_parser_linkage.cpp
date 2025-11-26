/**
 * Simple linkage test for ConfigParser
 * Verifies that ConfigParser implementation is properly compiled and linked
 */

#include <iostream>
#include <fstream>
#include <cassert>
#include <map>
#include <string>

// Forward declare the ConfigParser from pimid::config namespace
namespace pimid {
namespace config {
    class ConfigParser {
    public:
        ConfigParser();
        bool parseFile(const std::string& filename, std::map<std::string, std::string>& config);
        bool parseString(const std::string& yaml_content, std::map<std::string, std::string>& config);
    };
}
}

using namespace pimid;

// Helper function to create a test YAML config
void createTestConfig(const std::string& filename) {
    std::ofstream file(filename);
    file << "# Test configuration\n";
    file << "test:\n";
    file << "  key1: value1\n";
    file << "  key2: value2\n";
    file << "  nested:\n";
    file << "    key3: value3\n";
    file.close();
}

void test_config_parser_instantiation() {
    std::cout << "Test 1: ConfigParser instantiation... ";

    // This will fail with "undefined reference" if config_parser.cpp is not linked
    config::ConfigParser parser;

    std::cout << "PASSED\n";
    std::cout << "  - ConfigParser constructor linked successfully\n";
}

void test_config_parser_parse_file() {
    std::cout << "Test 2: ConfigParser::parseFile()... ";

    // Create test config
    createTestConfig("/tmp/test_config.yaml");

    // This will fail with "undefined reference" if config_parser.cpp is not linked
    config::ConfigParser parser;
    std::map<std::string, std::string> config_map;

    bool success = parser.parseFile("/tmp/test_config.yaml", config_map);

    if (!success) {
        std::cout << "FAILED\n";
        std::cout << "  - parseFile() returned false\n";
        exit(1);
    }

    if (config_map.empty()) {
        std::cout << "FAILED\n";
        std::cout << "  - Config map is empty\n";
        exit(1);
    }

    std::cout << "PASSED\n";
    std::cout << "  - parseFile() method linked and executed successfully\n";
    std::cout << "  - Parsed " << config_map.size() << " config entries\n";

    // Print some parsed values for verification
    for (const auto& kv : config_map) {
        std::cout << "    " << kv.first << " = " << kv.second << "\n";
    }
}

void test_config_parser_parse_string() {
    std::cout << "Test 3: ConfigParser::parseString()... ";

    std::string yaml_content =
        "config:\n"
        "  option1: enabled\n"
        "  option2: 42\n";

    config::ConfigParser parser;
    std::map<std::string, std::string> config_map;

    bool success = parser.parseString(yaml_content, config_map);

    if (!success) {
        std::cout << "FAILED\n";
        std::cout << "  - parseString() returned false\n";
        exit(1);
    }

    std::cout << "PASSED\n";
    std::cout << "  - parseString() method linked and executed successfully\n";
    std::cout << "  - Parsed " << config_map.size() << " config entries\n";
}

void test_missing_file_handling() {
    std::cout << "Test 4: Missing file handling... ";

    config::ConfigParser parser;
    std::map<std::string, std::string> config_map;

    bool success = parser.parseFile("/nonexistent/config.yaml", config_map);

    if (success) {
        std::cout << "FAILED\n";
        std::cout << "  - parseFile() should return false for missing files\n";
        exit(1);
    }

    std::cout << "PASSED\n";
    std::cout << "  - Gracefully handled missing config file\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "ConfigParser Linkage Test Suite\n";
    std::cout << "========================================\n\n";
    std::cout << "This test verifies that src/config/config_parser.cpp\n";
    std::cout << "is properly compiled into libpimid_lib.a\n\n";

    try {
        test_config_parser_instantiation();
        test_config_parser_parse_file();
        test_config_parser_parse_string();
        test_missing_file_handling();

        std::cout << "\n========================================\n";
        std::cout << "All tests PASSED!\n";
        std::cout << "ConfigParser is properly compiled and linked.\n";
        std::cout << "The critical build system issue is FIXED.\n";
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
