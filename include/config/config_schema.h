#ifndef PIMID_CONFIG_SCHEMA_H
#define PIMID_CONFIG_SCHEMA_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include <optional>

namespace pimid {
namespace config {

/**
 * Parameter types supported in configuration
 */
enum class ParameterType {
    STRING,
    INTEGER,
    UNSIGNED_INTEGER,
    FLOAT,
    BOOLEAN,
    ENUM,
    LIST,
    OBJECT,
    FILE_PATH,
    DIRECTORY_PATH
};

/**
 * Parameter validation rule
 */
struct ValidationRule {
    enum class RuleType {
        MIN_VALUE,
        MAX_VALUE,
        RANGE,
        REGEX,
        ONE_OF,
        FILE_EXISTS,
        DIR_EXISTS,
        CUSTOM
    };

    RuleType type;
    std::variant<int64_t, double, std::string, std::vector<std::string>> value;
    std::string error_message;

    ValidationRule(RuleType t) : type(t) {}
};

/**
 * Configuration parameter schema
 */
struct ParameterSchema {
    std::string name;
    std::string description;
    ParameterType type;
    bool required;
    std::variant<int64_t, double, std::string, bool> default_value;
    std::vector<ValidationRule> validation_rules;
    std::vector<std::string> allowed_values;  // For enum types
    std::string example;
    std::string category;  // For grouping in documentation

    ParameterSchema()
        : name(""), description(""), type(ParameterType::STRING),
          required(false), default_value(""), example(""), category("general") {}

    // Fluent interface for building schemas
    ParameterSchema& setName(const std::string& n) {
        name = n;
        return *this;
    }

    ParameterSchema& setDescription(const std::string& d) {
        description = d;
        return *this;
    }

    ParameterSchema& setType(ParameterType t) {
        type = t;
        return *this;
    }

    ParameterSchema& setRequired(bool r) {
        required = r;
        return *this;
    }

    template<typename T>
    ParameterSchema& setDefault(const T& val) {
        default_value = val;
        return *this;
    }

    ParameterSchema& addValidation(const ValidationRule& rule) {
        validation_rules.push_back(rule);
        return *this;
    }

    ParameterSchema& setAllowedValues(const std::vector<std::string>& values) {
        allowed_values = values;
        return *this;
    }

    ParameterSchema& setExample(const std::string& ex) {
        example = ex;
        return *this;
    }

    ParameterSchema& setCategory(const std::string& cat) {
        category = cat;
        return *this;
    }
};

/**
 * Configuration section schema
 * Groups related parameters together
 */
struct SectionSchema {
    std::string name;
    std::string description;
    std::vector<ParameterSchema> parameters;
    std::vector<SectionSchema> subsections;
    bool required;

    SectionSchema()
        : name(""), description(""), required(false) {}

    SectionSchema& setName(const std::string& n) {
        name = n;
        return *this;
    }

    SectionSchema& setDescription(const std::string& d) {
        description = d;
        return *this;
    }

    SectionSchema& setRequired(bool r) {
        required = r;
        return *this;
    }

    SectionSchema& addParameter(const ParameterSchema& param) {
        parameters.push_back(param);
        return *this;
    }

    SectionSchema& addSubsection(const SectionSchema& section) {
        subsections.push_back(section);
        return *this;
    }
};

/**
 * Complete configuration schema
 * Defines all valid configuration parameters
 */
class ConfigSchema {
public:
    ConfigSchema();

    // Add sections
    void addSection(const SectionSchema& section);

    // Query schema
    const SectionSchema* getSection(const std::string& name) const;
    const ParameterSchema* getParameter(const std::string& section,
                                        const std::string& param) const;

    // Validation
    bool validateValue(const ParameterSchema& param,
                       const std::string& value,
                       std::string& error) const;

    // Documentation generation
    std::string generateMarkdownDocs() const;
    std::string generateHTMLDocs() const;
    std::string generateYAMLTemplate() const;
    std::string generateJSONSchema() const;

    // Get all sections
    const std::vector<SectionSchema>& getSections() const { return sections_; }

private:
    std::vector<SectionSchema> sections_;
    std::map<std::string, const SectionSchema*> section_map_;

    void buildSectionMap();
};

/**
 * Configuration schema builder
 * Provides fluent interface for building schemas
 */
class SchemaBuilder {
public:
    SchemaBuilder();

    // Create PIMID standard schema
    static ConfigSchema createPIMIDSchema();

    // Create component-specific schemas
    static SectionSchema createHostSchema();
    static SectionSchema createDeviceSchema();
    static SectionSchema createMemorySchema();
    static SectionSchema createNetworkSchema();
    static SectionSchema createPowerSchema();
    static SectionSchema createSchedulerSchema();
    static SectionSchema createPluginSchema();

    // Helper functions for common parameter patterns
    static ParameterSchema createUnsignedIntParam(
        const std::string& name,
        const std::string& description,
        uint64_t default_val,
        std::optional<uint64_t> min = std::nullopt,
        std::optional<uint64_t> max = std::nullopt);

    static ParameterSchema createEnumParam(
        const std::string& name,
        const std::string& description,
        const std::string& default_val,
        const std::vector<std::string>& allowed_values);

    static ParameterSchema createFilePathParam(
        const std::string& name,
        const std::string& description,
        const std::string& default_val,
        bool must_exist = false);

    static ParameterSchema createBoolParam(
        const std::string& name,
        const std::string& description,
        bool default_val);

    static ParameterSchema createFloatParam(
        const std::string& name,
        const std::string& description,
        double default_val,
        std::optional<double> min = std::nullopt,
        std::optional<double> max = std::nullopt);
};

} // namespace config
} // namespace pimid

#endif // PIMID_CONFIG_SCHEMA_H
