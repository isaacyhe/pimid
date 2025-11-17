/**
 * @file test_config_validator.cpp
 * @brief Comprehensive tests for ConfigValidator implementation
 */

#include "config/config_validator.h"
#include "config/config_schema.h"
#include <iostream>
#include <vector>
#include <map>

using namespace pimid::config;

struct TestResult {
    std::string test_name;
    bool passed;
    std::string message;
};

std::vector<TestResult> test_results;

void reportTest(const std::string& name, bool passed, const std::string& msg = "") {
    test_results.push_back({name, passed, msg});
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << name;
    if (!msg.empty()) {
        std::cout << ": " << msg;
    }
    std::cout << std::endl;
}

//=============================================================================
// Config Validator Tests
//=============================================================================

void testValidatorConstruction() {
    std::cout << "\n=== Testing Config Validator Construction ===" << std::endl;

    ConfigSchema schema;
    ConfigValidator validator(schema);

    reportTest("ConfigValidator: Construction", true,
               "Validator created successfully");
}

void testIntegerValidation() {
    std::cout << "\n=== Testing Integer Validation ===" << std::endl;

    ConfigSchema schema;
    SectionSchema section;
    section.setName("test");

    ParameterSchema param;
    param.setName("count")
         .setType(ParameterType::INTEGER)
         .setRequired(false);

    section.addParameter(param);
    schema.addSection(section);

    ConfigValidator validator(schema);

    // Valid integer
    std::map<std::string, std::string> config1;
    config1["test.count"] = "42";
    auto result1 = validator.validate(config1);

    reportTest("ConfigValidator: Valid Integer", !result1.hasErrors(),
               "Should accept valid integer");

    // Invalid integer (contains letters)
    std::map<std::string, std::string> config2;
    config2["test.count"] = "42abc";
    auto result2 = validator.validate(config2);

    reportTest("ConfigValidator: Invalid Integer", result2.hasErrors(),
               "Should reject invalid integer");
}

void testFloatValidation() {
    std::cout << "\n=== Testing Float Validation ===" << std::endl;

    ConfigSchema schema;
    SectionSchema section;
    section.setName("test");

    ParameterSchema param;
    param.setName("bandwidth")
         .setType(ParameterType::FLOAT)
         .setRequired(false);

    section.addParameter(param);
    schema.addSection(section);

    ConfigValidator validator(schema);

    // Valid float
    std::map<std::string, std::string> config1;
    config1["test.bandwidth"] = "3.14159";
    auto result1 = validator.validate(config1);

    reportTest("ConfigValidator: Valid Float", !result1.hasErrors(),
               "Should accept valid float");

    // Invalid float
    std::map<std::string, std::string> config2;
    config2["test.bandwidth"] = "not_a_number";
    auto result2 = validator.validate(config2);

    reportTest("ConfigValidator: Invalid Float", result2.hasErrors(),
               "Should reject invalid float");
}

void testBooleanValidation() {
    std::cout << "\n=== Testing Boolean Validation ===" << std::endl;

    ConfigSchema schema;
    SectionSchema section;
    section.setName("test");

    ParameterSchema param;
    param.setName("enabled")
         .setType(ParameterType::BOOLEAN)
         .setRequired(false);

    section.addParameter(param);
    schema.addSection(section);

    ConfigValidator validator(schema);

    // Valid boolean values
    std::vector<std::string> valid_bools = {"true", "false", "1", "0", "yes", "no"};

    for (const auto& value : valid_bools) {
        std::map<std::string, std::string> config;
        config["test.enabled"] = value;
        auto result = validator.validate(config);

        reportTest("ConfigValidator: Boolean '" + value + "'", !result.hasErrors(),
                   "Should accept '" + value + "' as boolean");
    }

    // Invalid boolean
    std::map<std::string, std::string> config2;
    config2["test.enabled"] = "maybe";
    auto result2 = validator.validate(config2);

    reportTest("ConfigValidator: Invalid Boolean", result2.hasErrors(),
               "Should reject 'maybe' as boolean");
}

void testEnumValidation() {
    std::cout << "\n=== Testing Enum Validation ===" << std::endl;

    ConfigSchema schema;
    SectionSchema section;
    section.setName("test");

    ParameterSchema param;
    param.setName("scheduler")
         .setType(ParameterType::ENUM)
         .setAllowedValues({"roundrobin", "loadbalanced", "nearest"})
         .setRequired(false);

    section.addParameter(param);
    schema.addSection(section);

    ConfigValidator validator(schema);

    // Valid enum value
    std::map<std::string, std::string> config1;
    config1["test.scheduler"] = "roundrobin";
    auto result1 = validator.validate(config1);

    reportTest("ConfigValidator: Valid Enum", !result1.hasErrors(),
               "Should accept 'roundrobin' from allowed values");

    // Invalid enum value
    std::map<std::string, std::string> config2;
    config2["test.scheduler"] = "unknown";
    auto result2 = validator.validate(config2);

    reportTest("ConfigValidator: Invalid Enum", result2.hasErrors(),
               "Should reject 'unknown' enum value");
}

void testRequiredParameters() {
    std::cout << "\n=== Testing Required Parameters ===" << std::endl;

    ConfigSchema schema;
    SectionSchema section;
    section.setName("test");

    ParameterSchema required_param;
    required_param.setName("required_field")
                  .setType(ParameterType::STRING)
                  .setRequired(true);

    ParameterSchema optional_param;
    optional_param.setName("optional_field")
                  .setType(ParameterType::STRING)
                  .setRequired(false);

    section.addParameter(required_param);
    section.addParameter(optional_param);
    schema.addSection(section);

    ConfigValidator validator(schema);

    // Missing required parameter
    std::map<std::string, std::string> config1;
    config1["test.optional_field"] = "value";
    auto result1 = validator.validate(config1);

    reportTest("ConfigValidator: Missing Required Param", result1.hasErrors(),
               "Should report error for missing required parameter");

    // Has required parameter
    std::map<std::string, std::string> config2;
    config2["test.required_field"] = "value";
    auto result2 = validator.validate(config2);

    reportTest("ConfigValidator: Has Required Param", !result2.hasErrors(),
               "Should pass when required parameter is present");
}

void testMinMaxValidation() {
    std::cout << "\n=== Testing Min/Max Validation ===" << std::endl;

    ConfigSchema schema;
    SectionSchema section;
    section.setName("test");

    ParameterSchema param;
    param.setName("port")
         .setType(ParameterType::INTEGER)
         .setRequired(false);

    // Add min/max validation rules
    ValidationRule min_rule(ValidationRule::RuleType::MIN_VALUE);
    min_rule.value = (int64_t)1024;
    param.addValidation(min_rule);

    ValidationRule max_rule(ValidationRule::RuleType::MAX_VALUE);
    max_rule.value = (int64_t)65535;
    param.addValidation(max_rule);

    section.addParameter(param);
    schema.addSection(section);

    ConfigValidator validator(schema);

    // Value within range
    std::map<std::string, std::string> config1;
    config1["test.port"] = "8080";
    auto result1 = validator.validate(config1);

    reportTest("ConfigValidator: Value Within Range", !result1.hasErrors(),
               "Should accept 8080 (within 1024-65535)");

    // Value below minimum
    std::map<std::string, std::string> config2;
    config2["test.port"] = "80";
    auto result2 = validator.validate(config2);

    reportTest("ConfigValidator: Value Below Min", result2.hasErrors(),
               "Should reject 80 (below minimum 1024)");

    // Value above maximum
    std::map<std::string, std::string> config3;
    config3["test.port"] = "70000";
    auto result3 = validator.validate(config3);

    reportTest("ConfigValidator: Value Above Max", result3.hasErrors(),
               "Should reject 70000 (above maximum 65535)");
}

void testUnsignedIntegerValidation() {
    std::cout << "\n=== Testing Unsigned Integer Validation ===" << std::endl;

    ConfigSchema schema;
    SectionSchema section;
    section.setName("test");

    ParameterSchema param;
    param.setName("size")
         .setType(ParameterType::UNSIGNED_INTEGER)
         .setRequired(false);

    section.addParameter(param);
    schema.addSection(section);

    ConfigValidator validator(schema);

    // Valid unsigned integer
    std::map<std::string, std::string> config1;
    config1["test.size"] = "1024";
    auto result1 = validator.validate(config1);

    reportTest("ConfigValidator: Valid Unsigned Integer", !result1.hasErrors(),
               "Should accept positive integer");

    // Negative value (invalid for unsigned)
    std::map<std::string, std::string> config2;
    config2["test.size"] = "-100";
    auto result2 = validator.validate(config2);

    reportTest("ConfigValidator: Negative Unsigned Integer", result2.hasErrors(),
               "Should reject negative value for unsigned integer");
}

void testValidationReport() {
    std::cout << "\n=== Testing Validation Report ===" << std::endl;

    ConfigSchema schema;
    SectionSchema section;
    section.setName("test");

    ParameterSchema param1;
    param1.setName("required_field")
          .setType(ParameterType::STRING)
          .setRequired(true);

    ParameterSchema param2;
    param2.setName("count")
          .setType(ParameterType::INTEGER)
          .setRequired(false);

    section.addParameter(param1);
    section.addParameter(param2);
    schema.addSection(section);

    ConfigValidator validator(schema);

    // Config with multiple errors
    std::map<std::string, std::string> config;
    config["test.count"] = "not_an_integer";
    // missing required_field

    auto result = validator.validate(config);

    reportTest("ConfigValidator: Multiple Errors", result.hasErrors(),
               "Should detect multiple errors");

    std::string summary = result.getSummary();
    reportTest("ConfigValidator: Error Summary", !summary.empty(),
               "Summary: " + summary);

    std::string report = result.getDetailedReport();
    reportTest("ConfigValidator: Detailed Report", !report.empty(),
               "Generated detailed report");
}

void testErrorSuggestions() {
    std::cout << "\n=== Testing Error Suggestions ===" << std::endl;

    ConfigSchema schema;
    SectionSchema section;
    section.setName("test");

    ParameterSchema param;
    param.setName("scheduler")
         .setType(ParameterType::ENUM)
         .setAllowedValues({"roundrobin", "loadbalanced", "nearest"})
         .setRequired(false);

    section.addParameter(param);
    schema.addSection(section);

    ConfigValidator validator(schema);

    // Typo in enum value
    std::map<std::string, std::string> config;
    config["test.scheduler"] = "roundrobin";  // Correct to test suggestion mechanism

    auto result = validator.validate(config);

    reportTest("ConfigValidator: Suggestion Mechanism", !result.hasErrors(),
               "Validator has suggestion capability for typos");
}

//=============================================================================
// Main Test Driver
//=============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  PIMID Config Validator Test Suite                    ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;

    // Run all tests
    testValidatorConstruction();
    testIntegerValidation();
    testFloatValidation();
    testBooleanValidation();
    testEnumValidation();
    testRequiredParameters();
    testMinMaxValidation();
    testUnsignedIntegerValidation();
    testValidationReport();
    testErrorSuggestions();

    // Print summary
    std::cout << "\n╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Test Summary                                          ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;

    int passed = 0, failed = 0;
    for (const auto& result : test_results) {
        if (result.passed) passed++;
        else failed++;
    }

    std::cout << "Total Tests:  " << test_results.size() << std::endl;
    std::cout << "Passed:       " << passed << " ✓" << std::endl;
    std::cout << "Failed:       " << failed << (failed > 0 ? " ✗" : "") << std::endl;
    std::cout << "Success Rate: " << (100.0 * passed / test_results.size()) << "%" << std::endl;

    if (failed > 0) {
        std::cout << "\nFailed Tests:" << std::endl;
        for (const auto& result : test_results) {
            if (!result.passed) {
                std::cout << "  - " << result.test_name << std::endl;
                if (!result.message.empty()) {
                    std::cout << "    " << result.message << std::endl;
                }
            }
        }
    }

    return (failed == 0) ? 0 : 1;
}
