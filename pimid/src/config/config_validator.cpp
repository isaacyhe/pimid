#include "config/config_validator.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <filesystem>
#include <set>

namespace pimid {
namespace config {

//=============================================================================
// ValidationError Implementation
//=============================================================================

std::string ValidationError::toString() const {
    std::stringstream ss;

    switch (severity) {
        case Severity::ERROR:
            ss << "[ERROR] ";
            break;
        case Severity::WARNING:
            ss << "[WARNING] ";
            break;
        case Severity::INFO:
            ss << "[INFO] ";
            break;
    }

    ss << location << ": " << message;

    if (!suggestion.empty()) {
        ss << "\n  Suggestion: " << suggestion;
    }

    if (line_number >= 0) {
        ss << " (line " << line_number << ")";
    }

    return ss.str();
}

std::string ValidationError::toColorString() const {
    std::stringstream ss;

    // ANSI color codes
    const char* RED = "\033[1;31m";
    const char* YELLOW = "\033[1;33m";
    const char* CYAN = "\033[1;36m";
    const char* RESET = "\033[0m";

    switch (severity) {
        case Severity::ERROR:
            ss << RED << "[ERROR]" << RESET << " ";
            break;
        case Severity::WARNING:
            ss << YELLOW << "[WARNING]" << RESET << " ";
            break;
        case Severity::INFO:
            ss << CYAN << "[INFO]" << RESET << " ";
            break;
    }

    ss << location << ": " << message;

    if (!suggestion.empty()) {
        ss << "\n  " << CYAN << "Suggestion:" << RESET << " " << suggestion;
    }

    if (line_number >= 0) {
        ss << " (line " << line_number << ")";
    }

    return ss.str();
}

//=============================================================================
// ValidationResult Implementation
//=============================================================================

std::string ValidationResult::getSummary() const {
    std::stringstream ss;

    if (valid && warnings.empty()) {
        ss << "Configuration is valid!";
    } else if (!valid) {
        ss << "Configuration has " << errors.size() << " error(s)";
        if (!warnings.empty()) {
            ss << " and " << warnings.size() << " warning(s)";
        }
    } else {
        ss << "Configuration is valid but has " << warnings.size() << " warning(s)";
    }

    return ss.str();
}

std::string ValidationResult::getDetailedReport() const {
    std::stringstream ss;

    ss << "=== Configuration Validation Report ===" << std::endl;
    ss << getSummary() << std::endl;
    ss << std::endl;

    if (!errors.empty()) {
        ss << "Errors:" << std::endl;
        for (const auto& error : errors) {
            ss << "  " << error.toString() << std::endl;
        }
        ss << std::endl;
    }

    if (!warnings.empty()) {
        ss << "Warnings:" << std::endl;
        for (const auto& warning : warnings) {
            ss << "  " << warning.toString() << std::endl;
        }
    }

    return ss.str();
}

//=============================================================================
// ConfigValidator Implementation
//=============================================================================

ConfigValidator::ConfigValidator(const ConfigSchema& schema)
    : schema_(schema), strict_mode_(false),
      check_file_existence_(true), allow_unknown_params_(false) {
}

ValidationResult ConfigValidator::validate(
    const std::map<std::string, std::string>& config) {

    ValidationResult result;

    // Validate each section in schema
    for (const auto& section : schema_.getSections()) {
        validateSection(section, config, result, "");
    }

    // Check for unknown parameters if not allowed
    if (!allow_unknown_params_) {
        // Build set of all valid parameter paths from schema
        std::set<std::string> valid_paths;
        std::function<void(const SectionSchema&, const std::string&)> collectPaths;
        collectPaths = [&](const SectionSchema& section, const std::string& prefix) {
            std::string section_prefix = prefix.empty() ?
                section.name : prefix + "." + section.name;

            // Add parameter paths
            for (const auto& param : section.parameters) {
                valid_paths.insert(section_prefix + "." + param.name);
            }

            // Process subsections recursively
            for (const auto& subsection : section.subsections) {
                collectPaths(subsection, section_prefix);
            }
        };

        for (const auto& section : schema_.getSections()) {
            collectPaths(section, "");
        }

        // Check for unknown parameters in config
        for (const auto& [key, value] : config) {
            // Check if this key or any prefix of it is valid
            bool found = false;

            // Direct match
            if (valid_paths.find(key) != valid_paths.end()) {
                found = true;
            }

            // Check if any valid path starts with this key (it's a parent)
            if (!found) {
                for (const auto& valid_path : valid_paths) {
                    if (valid_path.find(key + ".") == 0 || key.find(valid_path) == 0) {
                        found = true;
                        break;
                    }
                }
            }

            if (!found && !valid_paths.empty()) {
                // Find closest match for suggestion
                std::vector<std::string> valid_vec(valid_paths.begin(), valid_paths.end());
                std::string suggestion = suggestCorrection(key, valid_vec);

                ValidationError error(
                    strict_mode_ ? ValidationError::Severity::ERROR : ValidationError::Severity::WARNING,
                    key,
                    "Unknown configuration parameter",
                    suggestion.empty() ? "" : "Did you mean: " + suggestion + "?"
                );
                result.addError(error);
            }
        }
    }

    return result;
}

ValidationResult ConfigValidator::validateFile(const std::string& yaml_file) {
    ValidationResult result;

    // Check file existence
    if (!std::filesystem::exists(yaml_file)) {
        ValidationError error(
            ValidationError::Severity::ERROR,
            yaml_file,
            "Configuration file does not exist",
            "Check the file path and ensure it exists"
        );
        result.addError(error);
        return result;
    }

    // Read file
    std::ifstream file(yaml_file);
    if (!file.is_open()) {
        ValidationError error(
            ValidationError::Severity::ERROR,
            yaml_file,
            "Cannot open configuration file",
            "Check file permissions"
        );
        result.addError(error);
        return result;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string yaml_content = buffer.str();

    // Validate YAML content
    return validateYAML(yaml_content);
}

ValidationResult ConfigValidator::validateYAML(const std::string& yaml_content) {
    ValidationResult result;

    try {
        // Parse YAML content
        YAML::Node root = YAML::Load(yaml_content);

        // Convert YAML to flat map
        std::map<std::string, std::string> config;
        std::function<void(const YAML::Node&, const std::string&)> flattenNode;
        flattenNode = [&](const YAML::Node& node, const std::string& prefix) {
            if (!node.IsDefined() || node.IsNull()) {
                return;
            }

            if (node.IsScalar()) {
                config[prefix] = node.as<std::string>();
                return;
            }

            if (node.IsSequence()) {
                // Store list as comma-separated values
                std::ostringstream oss;
                for (size_t i = 0; i < node.size(); ++i) {
                    if (i > 0) oss << ",";
                    if (node[i].IsScalar()) {
                        oss << node[i].as<std::string>();
                    }
                }
                config[prefix] = oss.str();
                return;
            }

            if (node.IsMap()) {
                for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
                    std::string key = it->first.as<std::string>();
                    std::string full_key = prefix.empty() ? key : prefix + "." + key;
                    flattenNode(it->second, full_key);
                }
            }
        };

        flattenNode(root, "");

        // Validate the parsed config
        return validate(config);

    } catch (const YAML::ParserException& e) {
        ValidationError error(
            ValidationError::Severity::ERROR,
            "yaml_parser",
            "YAML syntax error: " + std::string(e.what()),
            "Check YAML syntax at the reported line"
        );
        error.line_number = e.mark.line;
        result.addError(error);
    } catch (const YAML::Exception& e) {
        ValidationError error(
            ValidationError::Severity::ERROR,
            "yaml_parser",
            "YAML parsing error: " + std::string(e.what()),
            "Verify YAML file format"
        );
        result.addError(error);
    } catch (const std::exception& e) {
        ValidationError error(
            ValidationError::Severity::ERROR,
            "yaml_parser",
            "Error processing YAML: " + std::string(e.what()),
            "Check configuration content"
        );
        result.addError(error);
    }

    return result;
}

bool ConfigValidator::validateParameter(
    const ParameterSchema& param,
    const std::string& value,
    ValidationResult& result,
    const std::string& location) {

    // Validate type
    if (!validateType(param, value, result, location)) {
        return false;
    }

    // Validate rules
    if (!validateRules(param, value, result, location)) {
        return false;
    }

    return true;
}

bool ConfigValidator::validateSection(
    const SectionSchema& section,
    const std::map<std::string, std::string>& config,
    ValidationResult& result,
    const std::string& prefix) {

    std::string section_prefix = prefix.empty() ?
        section.name : prefix + "." + section.name;

    // Check required parameters
    checkRequiredParameters(section, config, result, section_prefix);

    // Validate each parameter in this section
    for (const auto& param : section.parameters) {
        std::string param_path = section_prefix + "." + param.name;

        auto it = config.find(param_path);
        if (it != config.end()) {
            validateParameter(param, it->second, result, param_path);
        }
    }

    // Validate subsections recursively
    for (const auto& subsection : section.subsections) {
        validateSection(subsection, config, result, section_prefix);
    }

    return true;
}

bool ConfigValidator::checkRequiredParameters(
    const SectionSchema& section,
    const std::map<std::string, std::string>& config,
    ValidationResult& result,
    const std::string& prefix) {

    bool all_present = true;

    for (const auto& param : section.parameters) {
        if (param.required) {
            std::string param_path = prefix + "." + param.name;

            if (config.find(param_path) == config.end()) {
                ValidationError error(
                    ValidationError::Severity::ERROR,
                    param_path,
                    "Required parameter is missing",
                    "Add " + param_path + " = " + param.example
                );
                result.addError(error);
                all_present = false;
            }
        }
    }

    return all_present;
}

bool ConfigValidator::validateType(
    const ParameterSchema& param,
    const std::string& value,
    ValidationResult& result,
    const std::string& location) {

    try {
        switch (param.type) {
            case ParameterType::INTEGER:
            case ParameterType::UNSIGNED_INTEGER: {
                // Try to parse as integer
                size_t pos;
                int64_t int_val = std::stoll(value, &pos);

                if (pos != value.length()) {
                    ValidationError error(
                        ValidationError::Severity::ERROR,
                        location,
                        "Invalid integer value: " + value,
                        "Expected an integer"
                    );
                    result.addError(error);
                    return false;
                }

                if (param.type == ParameterType::UNSIGNED_INTEGER && int_val < 0) {
                    ValidationError error(
                        ValidationError::Severity::ERROR,
                        location,
                        "Value must be non-negative: " + value,
                        "Use a positive integer value"
                    );
                    result.addError(error);
                    return false;
                }
                break;
            }

            case ParameterType::FLOAT: {
                // Try to parse as float
                size_t pos;
                std::stod(value, &pos);

                if (pos != value.length()) {
                    ValidationError error(
                        ValidationError::Severity::ERROR,
                        location,
                        "Invalid float value: " + value,
                        "Expected a floating-point number"
                    );
                    result.addError(error);
                    return false;
                }
                break;
            }

            case ParameterType::BOOLEAN: {
                // Check if valid boolean
                std::string lower_val = value;
                std::transform(lower_val.begin(), lower_val.end(),
                             lower_val.begin(), ::tolower);

                if (lower_val != "true" && lower_val != "false" &&
                    lower_val != "1" && lower_val != "0" &&
                    lower_val != "yes" && lower_val != "no") {
                    ValidationError error(
                        ValidationError::Severity::ERROR,
                        location,
                        "Invalid boolean value: " + value,
                        "Expected true/false, yes/no, or 1/0"
                    );
                    result.addError(error);
                    return false;
                }
                break;
            }

            case ParameterType::ENUM: {
                // Check if value is in allowed_values
                if (!param.allowed_values.empty()) {
                    auto it = std::find(param.allowed_values.begin(),
                                      param.allowed_values.end(), value);
                    if (it == param.allowed_values.end()) {
                        std::string suggestion = suggestCorrection(value,
                                                                  param.allowed_values);
                        ValidationError error(
                            ValidationError::Severity::ERROR,
                            location,
                            "Invalid enum value: " + value,
                            "Did you mean: " + suggestion + "?"
                        );
                        result.addError(error);
                        return false;
                    }
                }
                break;
            }

            case ParameterType::FILE_PATH:
            case ParameterType::DIRECTORY_PATH: {
                if (check_file_existence_) {
                    bool exists = param.type == ParameterType::FILE_PATH ?
                        std::filesystem::exists(value) :
                        std::filesystem::is_directory(value);

                    if (!exists) {
                        ValidationError error(
                            ValidationError::Severity::WARNING,
                            location,
                            (param.type == ParameterType::FILE_PATH ?
                             "File does not exist: " : "Directory does not exist: ") + value,
                            "Create the path or update configuration"
                        );
                        result.addError(error);
                    }
                }
                break;
            }

            default:
                // STRING, LIST, OBJECT - no type validation needed
                break;
        }
    } catch (const std::exception& e) {
        ValidationError error(
            ValidationError::Severity::ERROR,
            location,
            "Type validation failed: " + std::string(e.what()),
            "Check parameter type and value"
        );
        result.addError(error);
        return false;
    }

    return true;
}

bool ConfigValidator::validateRules(
    const ParameterSchema& param,
    const std::string& value,
    ValidationResult& result,
    const std::string& location) {

    // Validate each rule
    for (const auto& rule : param.validation_rules) {
        switch (rule.type) {
            case ValidationRule::RuleType::MIN_VALUE: {
                try {
                    if (param.type == ParameterType::FLOAT) {
                        double val = std::stod(value);
                        double min_val = std::get<double>(rule.value);
                        if (val < min_val) {
                            ValidationError error(
                                ValidationError::Severity::ERROR,
                                location,
                                "Value " + value + " is below minimum " +
                                std::to_string(min_val),
                                "Use a value >= " + std::to_string(min_val)
                            );
                            result.addError(error);
                            return false;
                        }
                    } else {
                        int64_t val = std::stoll(value);
                        int64_t min_val = std::get<int64_t>(rule.value);
                        if (val < min_val) {
                            ValidationError error(
                                ValidationError::Severity::ERROR,
                                location,
                                "Value " + value + " is below minimum " +
                                std::to_string(min_val),
                                "Use a value >= " + std::to_string(min_val)
                            );
                            result.addError(error);
                            return false;
                        }
                    }
                } catch (...) {
                    // Skip validation if parsing fails
                }
                break;
            }

            case ValidationRule::RuleType::MAX_VALUE: {
                try {
                    if (param.type == ParameterType::FLOAT) {
                        double val = std::stod(value);
                        double max_val = std::get<double>(rule.value);
                        if (val > max_val) {
                            ValidationError error(
                                ValidationError::Severity::ERROR,
                                location,
                                "Value " + value + " exceeds maximum " +
                                std::to_string(max_val),
                                "Use a value <= " + std::to_string(max_val)
                            );
                            result.addError(error);
                            return false;
                        }
                    } else {
                        int64_t val = std::stoll(value);
                        int64_t max_val = std::get<int64_t>(rule.value);
                        if (val > max_val) {
                            ValidationError error(
                                ValidationError::Severity::ERROR,
                                location,
                                "Value " + value + " exceeds maximum " +
                                std::to_string(max_val),
                                "Use a value <= " + std::to_string(max_val)
                            );
                            result.addError(error);
                            return false;
                        }
                    }
                } catch (...) {
                    // Skip validation if parsing fails
                }
                break;
            }

            default:
                // Other rule types not yet implemented
                break;
        }
    }

    return true;
}

std::string ConfigValidator::suggestCorrection(
    const std::string& invalid_value,
    const std::vector<std::string>& valid_values) {

    if (valid_values.empty()) {
        return "";
    }

    // Simple Levenshtein-based suggestion (simplified version)
    // Find the valid value with most characters in common
    std::string best_match = valid_values[0];
    size_t max_common = 0;

    for (const auto& valid : valid_values) {
        size_t common = 0;
        for (char c : invalid_value) {
            if (valid.find(c) != std::string::npos) {
                common++;
            }
        }
        if (common > max_common) {
            max_common = common;
            best_match = valid;
        }
    }

    return best_match;
}

} // namespace config
} // namespace pimid
