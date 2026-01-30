#ifndef PIMID_CONFIG_VALIDATOR_H
#define PIMID_CONFIG_VALIDATOR_H

#include "config/config_schema.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace pimid {
namespace config {

/**
 * Validation error with location and suggestion
 */
struct ValidationError {
    enum class Severity {
        ERROR,
        WARNING,
        INFO
    };

    Severity severity;
    std::string location;      // e.g., "memory.dram.frequency"
    std::string message;
    std::string suggestion;    // Suggested fix
    int line_number;           // Line in YAML file

    ValidationError()
        : severity(Severity::ERROR), location(""), message(""),
          suggestion(""), line_number(-1) {}

    ValidationError(Severity sev, const std::string& loc,
                    const std::string& msg, const std::string& sug = "")
        : severity(sev), location(loc), message(msg),
          suggestion(sug), line_number(-1) {}

    std::string toString() const;
    std::string toColorString() const;  // With ANSI colors
};

/**
 * Validation result
 */
struct ValidationResult {
    bool valid;
    std::vector<ValidationError> errors;
    std::vector<ValidationError> warnings;

    ValidationResult() : valid(true) {}

    void addError(const ValidationError& error) {
        if (error.severity == ValidationError::Severity::ERROR) {
            errors.push_back(error);
            valid = false;
        } else if (error.severity == ValidationError::Severity::WARNING) {
            warnings.push_back(error);
        }
    }

    bool hasErrors() const { return !errors.empty(); }
    bool hasWarnings() const { return !warnings.empty(); }

    std::string getSummary() const;
    std::string getDetailedReport() const;
};

/**
 * Configuration validator
 * Validates configuration against schema
 */
class ConfigValidator {
public:
    ConfigValidator(const ConfigSchema& schema);

    // Validation
    ValidationResult validate(const std::map<std::string, std::string>& config);
    ValidationResult validateFile(const std::string& yaml_file);
    ValidationResult validateYAML(const std::string& yaml_content);

    // Validation options
    void setStrictMode(bool strict) { strict_mode_ = strict; }
    void setCheckFileExistence(bool check) { check_file_existence_ = check; }
    void setAllowUnknownParameters(bool allow) { allow_unknown_params_ = allow; }

    // Get schema
    const ConfigSchema& getSchema() const { return schema_; }

private:
    ConfigSchema schema_;
    bool strict_mode_;
    bool check_file_existence_;
    bool allow_unknown_params_;

    // Validation helpers
    bool validateParameter(const ParameterSchema& param,
                           const std::string& value,
                           ValidationResult& result,
                           const std::string& location);

    bool validateSection(const SectionSchema& section,
                         const std::map<std::string, std::string>& config,
                         ValidationResult& result,
                         const std::string& prefix);

    bool checkRequiredParameters(const SectionSchema& section,
                                 const std::map<std::string, std::string>& config,
                                 ValidationResult& result,
                                 const std::string& prefix);

    bool validateType(const ParameterSchema& param,
                      const std::string& value,
                      ValidationResult& result,
                      const std::string& location);

    bool validateRules(const ParameterSchema& param,
                       const std::string& value,
                       ValidationResult& result,
                       const std::string& location);

    std::string suggestCorrection(const std::string& invalid_value,
                                  const std::vector<std::string>& valid_values);
};

/**
 * Configuration builder with validation
 * Fluent interface for building valid configurations
 */
class ConfigBuilder {
public:
    ConfigBuilder(const ConfigSchema& schema);

    // Set parameters
    ConfigBuilder& set(const std::string& path, const std::string& value);
    ConfigBuilder& set(const std::string& path, int64_t value);
    ConfigBuilder& set(const std::string& path, double value);
    ConfigBuilder& set(const std::string& path, bool value);

    // Load from file/string
    ConfigBuilder& loadFromFile(const std::string& yaml_file);
    ConfigBuilder& loadFromYAML(const std::string& yaml_content);
    ConfigBuilder& loadFromMap(const std::map<std::string, std::string>& config);

    // Merge configurations
    ConfigBuilder& merge(const ConfigBuilder& other);

    // Validation
    ValidationResult validate();
    bool isValid();

    // Export
    std::map<std::string, std::string> build();
    std::string toYAML() const;
    std::string toJSON() const;
    bool saveToFile(const std::string& filename);

    // Query
    std::string get(const std::string& path, const std::string& default_val = "") const;
    int64_t getInt(const std::string& path, int64_t default_val = 0) const;
    double getFloat(const std::string& path, double default_val = 0.0) const;
    bool getBool(const std::string& path, bool default_val = false) const;

    // Get parameters by category
    std::map<std::string, std::string> getCategory(const std::string& category) const;

private:
    ConfigSchema schema_;
    std::map<std::string, std::string> config_;
    ValidationResult last_validation_;

    void applyDefaults();
    std::string expandPath(const std::string& path) const;
};

/**
 * Configuration template generator
 * Creates configuration templates with documentation
 */
class ConfigTemplateGenerator {
public:
    ConfigTemplateGenerator(const ConfigSchema& schema);

    // Generate templates
    std::string generateMinimalYAML() const;      // Only required params
    std::string generateFullYAML() const;         // All params with defaults
    std::string generateAnnotatedYAML() const;    // With inline comments
    std::string generateExampleYAML(const std::string& use_case) const;

    // Generate documentation
    std::string generateMarkdown() const;
    std::string generateHTML() const;
    std::string generateManPage() const;

    // Interactive generation
    std::string generateInteractive() const;  // Prompts user for values

private:
    ConfigSchema schema_;

    std::string generateSectionYAML(const SectionSchema& section,
                                     int indent_level,
                                     bool include_optional,
                                     bool include_comments) const;
};

} // namespace config
} // namespace pimid

#endif // PIMID_CONFIG_VALIDATOR_H
