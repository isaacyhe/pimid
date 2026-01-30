#include "config/config_schema.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace pimid {
namespace config {

namespace {

// Helper to convert ParameterType to string
std::string paramTypeToString(ParameterType type) {
    switch (type) {
        case ParameterType::STRING: return "string";
        case ParameterType::INTEGER: return "integer";
        case ParameterType::UNSIGNED_INTEGER: return "unsigned integer";
        case ParameterType::FLOAT: return "float";
        case ParameterType::BOOLEAN: return "boolean";
        case ParameterType::ENUM: return "enum";
        case ParameterType::LIST: return "list";
        case ParameterType::OBJECT: return "object";
        case ParameterType::FILE_PATH: return "file path";
        case ParameterType::DIRECTORY_PATH: return "directory path";
        default: return "unknown";
    }
}

// Helper to convert ParameterType to JSON Schema type
std::string paramTypeToJSONSchemaType(ParameterType type) {
    switch (type) {
        case ParameterType::STRING:
        case ParameterType::ENUM:
        case ParameterType::FILE_PATH:
        case ParameterType::DIRECTORY_PATH:
            return "string";
        case ParameterType::INTEGER:
        case ParameterType::UNSIGNED_INTEGER:
            return "integer";
        case ParameterType::FLOAT:
            return "number";
        case ParameterType::BOOLEAN:
            return "boolean";
        case ParameterType::LIST:
            return "array";
        case ParameterType::OBJECT:
            return "object";
        default:
            return "string";
    }
}

// Helper to get default value as string
std::string getDefaultAsString(const std::variant<int64_t, double, std::string, bool>& value) {
    if (std::holds_alternative<int64_t>(value)) {
        return std::to_string(std::get<int64_t>(value));
    } else if (std::holds_alternative<double>(value)) {
        std::ostringstream oss;
        oss << std::get<double>(value);
        return oss.str();
    } else if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    } else if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    }
    return "";
}

// Escape string for HTML
std::string escapeHTML(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '&': result += "&amp;"; break;
            case '"': result += "&quot;"; break;
            default: result += c;
        }
    }
    return result;
}

// Escape string for JSON
std::string escapeJSON(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

} // anonymous namespace

// ConfigSchema constructor - minimal implementation
ConfigSchema::ConfigSchema() {
    // Initialize with empty schema
    // Full schema definition can be added later as needed
}

void ConfigSchema::addSection(const SectionSchema& section) {
    sections_.push_back(section);
    buildSectionMap();
}

const SectionSchema* ConfigSchema::getSection(const std::string& name) const {
    auto it = section_map_.find(name);
    return (it != section_map_.end()) ? it->second : nullptr;
}

const ParameterSchema* ConfigSchema::getParameter(const std::string& section,
                                                   const std::string& param) const {
    const SectionSchema* sec = getSection(section);
    if (!sec) return nullptr;

    for (const auto& p : sec->parameters) {
        if (p.name == param) return &p;
    }
    return nullptr;
}

bool ConfigSchema::validateValue(const ParameterSchema& param,
                                  const std::string& value,
                                  std::string& error) const {
    // Basic validation - can be extended
    if (param.required && value.empty()) {
        error = "Parameter " + param.name + " is required";
        return false;
    }
    return true;
}

std::string ConfigSchema::generateMarkdownDocs() const {
    std::ostringstream md;

    md << "# PIMID Configuration Schema\n\n";
    md << "This document describes all available configuration parameters for the PIMID simulator.\n\n";
    md << "## Table of Contents\n\n";

    // Generate TOC
    for (const auto& section : sections_) {
        md << "- [" << section.name << "](#" << section.name << ")\n";
        for (const auto& subsec : section.subsections) {
            md << "  - [" << subsec.name << "](#" << section.name << "-" << subsec.name << ")\n";
        }
    }
    md << "\n---\n\n";

    // Generate documentation for each section
    for (const auto& section : sections_) {
        md << "## " << section.name << "\n\n";
        if (!section.description.empty()) {
            md << section.description << "\n\n";
        }
        if (section.required) {
            md << "**Required:** Yes\n\n";
        }

        // Parameters table
        if (!section.parameters.empty()) {
            md << "| Parameter | Type | Required | Default | Description |\n";
            md << "|-----------|------|----------|---------|-------------|\n";

            for (const auto& param : section.parameters) {
                md << "| `" << param.name << "` | "
                   << paramTypeToString(param.type) << " | "
                   << (param.required ? "Yes" : "No") << " | "
                   << "`" << getDefaultAsString(param.default_value) << "` | "
                   << param.description << " |\n";
            }
            md << "\n";
        }

        // Detailed parameter descriptions
        if (!section.parameters.empty()) {
            md << "### Parameter Details\n\n";
            for (const auto& param : section.parameters) {
                md << "#### `" << section.name << "." << param.name << "`\n\n";
                md << param.description << "\n\n";
                md << "- **Type:** " << paramTypeToString(param.type) << "\n";
                md << "- **Required:** " << (param.required ? "Yes" : "No") << "\n";
                md << "- **Default:** `" << getDefaultAsString(param.default_value) << "`\n";

                if (!param.allowed_values.empty()) {
                    md << "- **Allowed Values:** ";
                    for (size_t i = 0; i < param.allowed_values.size(); ++i) {
                        if (i > 0) md << ", ";
                        md << "`" << param.allowed_values[i] << "`";
                    }
                    md << "\n";
                }

                if (!param.example.empty()) {
                    md << "- **Example:** `" << param.example << "`\n";
                }

                // Validation rules
                if (!param.validation_rules.empty()) {
                    md << "- **Validation:**\n";
                    for (const auto& rule : param.validation_rules) {
                        switch (rule.type) {
                            case ValidationRule::RuleType::MIN_VALUE:
                                if (std::holds_alternative<int64_t>(rule.value)) {
                                    md << "  - Minimum: " << std::get<int64_t>(rule.value) << "\n";
                                } else if (std::holds_alternative<double>(rule.value)) {
                                    md << "  - Minimum: " << std::get<double>(rule.value) << "\n";
                                }
                                break;
                            case ValidationRule::RuleType::MAX_VALUE:
                                if (std::holds_alternative<int64_t>(rule.value)) {
                                    md << "  - Maximum: " << std::get<int64_t>(rule.value) << "\n";
                                } else if (std::holds_alternative<double>(rule.value)) {
                                    md << "  - Maximum: " << std::get<double>(rule.value) << "\n";
                                }
                                break;
                            case ValidationRule::RuleType::REGEX:
                                if (std::holds_alternative<std::string>(rule.value)) {
                                    md << "  - Pattern: `" << std::get<std::string>(rule.value) << "`\n";
                                }
                                break;
                            case ValidationRule::RuleType::FILE_EXISTS:
                                md << "  - File must exist\n";
                                break;
                            case ValidationRule::RuleType::DIR_EXISTS:
                                md << "  - Directory must exist\n";
                                break;
                            default:
                                break;
                        }
                    }
                }
                md << "\n";
            }
        }

        // Subsections
        for (const auto& subsec : section.subsections) {
            md << "### " << section.name << "." << subsec.name << "\n\n";
            if (!subsec.description.empty()) {
                md << subsec.description << "\n\n";
            }

            if (!subsec.parameters.empty()) {
                md << "| Parameter | Type | Required | Default | Description |\n";
                md << "|-----------|------|----------|---------|-------------|\n";

                for (const auto& param : subsec.parameters) {
                    md << "| `" << param.name << "` | "
                       << paramTypeToString(param.type) << " | "
                       << (param.required ? "Yes" : "No") << " | "
                       << "`" << getDefaultAsString(param.default_value) << "` | "
                       << param.description << " |\n";
                }
                md << "\n";
            }
        }

        md << "---\n\n";
    }

    return md.str();
}

std::string ConfigSchema::generateHTMLDocs() const {
    std::ostringstream html;

    html << "<!DOCTYPE html>\n"
         << "<html lang=\"en\">\n"
         << "<head>\n"
         << "  <meta charset=\"UTF-8\">\n"
         << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
         << "  <title>PIMID Configuration Schema</title>\n"
         << "  <style>\n"
         << "    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
         << "           line-height: 1.6; max-width: 1200px; margin: 0 auto; padding: 20px; }\n"
         << "    h1 { color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 10px; }\n"
         << "    h2 { color: #34495e; margin-top: 40px; }\n"
         << "    h3 { color: #7f8c8d; }\n"
         << "    table { border-collapse: collapse; width: 100%; margin: 20px 0; }\n"
         << "    th, td { border: 1px solid #ddd; padding: 12px; text-align: left; }\n"
         << "    th { background-color: #3498db; color: white; }\n"
         << "    tr:nth-child(even) { background-color: #f9f9f9; }\n"
         << "    tr:hover { background-color: #f1f1f1; }\n"
         << "    code { background-color: #f4f4f4; padding: 2px 6px; border-radius: 3px; "
         << "           font-family: 'Monaco', 'Consolas', monospace; }\n"
         << "    .required { color: #e74c3c; font-weight: bold; }\n"
         << "    .optional { color: #27ae60; }\n"
         << "    .param-detail { background: #f8f9fa; padding: 15px; margin: 10px 0; border-radius: 5px; }\n"
         << "    .toc { background: #ecf0f1; padding: 20px; border-radius: 5px; margin-bottom: 30px; }\n"
         << "    .toc ul { list-style-type: none; padding-left: 20px; }\n"
         << "    .toc a { text-decoration: none; color: #2980b9; }\n"
         << "    .toc a:hover { text-decoration: underline; }\n"
         << "  </style>\n"
         << "</head>\n"
         << "<body>\n";

    html << "<h1>PIMID Configuration Schema</h1>\n";
    html << "<p>Complete reference for all configuration parameters in the PIMID simulator.</p>\n";

    // Table of Contents
    html << "<div class=\"toc\">\n"
         << "  <h2>Table of Contents</h2>\n"
         << "  <ul>\n";

    for (const auto& section : sections_) {
        html << "    <li><a href=\"#" << section.name << "\">" << escapeHTML(section.name) << "</a>\n";
        if (!section.subsections.empty()) {
            html << "      <ul>\n";
            for (const auto& subsec : section.subsections) {
                html << "        <li><a href=\"#" << section.name << "-" << subsec.name << "\">"
                     << escapeHTML(subsec.name) << "</a></li>\n";
            }
            html << "      </ul>\n";
        }
        html << "    </li>\n";
    }
    html << "  </ul>\n</div>\n";

    // Generate documentation for each section
    for (const auto& section : sections_) {
        html << "<h2 id=\"" << section.name << "\">" << escapeHTML(section.name) << "</h2>\n";

        if (!section.description.empty()) {
            html << "<p>" << escapeHTML(section.description) << "</p>\n";
        }

        if (section.required) {
            html << "<p><span class=\"required\">Required Section</span></p>\n";
        }

        // Parameters table
        if (!section.parameters.empty()) {
            html << "<table>\n"
                 << "  <thead>\n"
                 << "    <tr><th>Parameter</th><th>Type</th><th>Required</th><th>Default</th><th>Description</th></tr>\n"
                 << "  </thead>\n"
                 << "  <tbody>\n";

            for (const auto& param : section.parameters) {
                html << "    <tr>\n"
                     << "      <td><code>" << escapeHTML(param.name) << "</code></td>\n"
                     << "      <td>" << paramTypeToString(param.type) << "</td>\n"
                     << "      <td class=\"" << (param.required ? "required" : "optional") << "\">"
                     << (param.required ? "Yes" : "No") << "</td>\n"
                     << "      <td><code>" << escapeHTML(getDefaultAsString(param.default_value)) << "</code></td>\n"
                     << "      <td>" << escapeHTML(param.description) << "</td>\n"
                     << "    </tr>\n";
            }

            html << "  </tbody>\n</table>\n";

            // Parameter details
            html << "<h3>Parameter Details</h3>\n";
            for (const auto& param : section.parameters) {
                html << "<div class=\"param-detail\">\n"
                     << "  <h4><code>" << escapeHTML(section.name) << "." << escapeHTML(param.name) << "</code></h4>\n"
                     << "  <p>" << escapeHTML(param.description) << "</p>\n"
                     << "  <ul>\n"
                     << "    <li><strong>Type:</strong> " << paramTypeToString(param.type) << "</li>\n"
                     << "    <li><strong>Required:</strong> " << (param.required ? "Yes" : "No") << "</li>\n"
                     << "    <li><strong>Default:</strong> <code>" << escapeHTML(getDefaultAsString(param.default_value)) << "</code></li>\n";

                if (!param.allowed_values.empty()) {
                    html << "    <li><strong>Allowed Values:</strong> ";
                    for (size_t i = 0; i < param.allowed_values.size(); ++i) {
                        if (i > 0) html << ", ";
                        html << "<code>" << escapeHTML(param.allowed_values[i]) << "</code>";
                    }
                    html << "</li>\n";
                }

                if (!param.example.empty()) {
                    html << "    <li><strong>Example:</strong> <code>" << escapeHTML(param.example) << "</code></li>\n";
                }

                html << "  </ul>\n</div>\n";
            }
        }

        // Subsections
        for (const auto& subsec : section.subsections) {
            html << "<h3 id=\"" << section.name << "-" << subsec.name << "\">"
                 << escapeHTML(section.name) << "." << escapeHTML(subsec.name) << "</h3>\n";

            if (!subsec.description.empty()) {
                html << "<p>" << escapeHTML(subsec.description) << "</p>\n";
            }

            if (!subsec.parameters.empty()) {
                html << "<table>\n"
                     << "  <thead>\n"
                     << "    <tr><th>Parameter</th><th>Type</th><th>Required</th><th>Default</th><th>Description</th></tr>\n"
                     << "  </thead>\n"
                     << "  <tbody>\n";

                for (const auto& param : subsec.parameters) {
                    html << "    <tr>\n"
                         << "      <td><code>" << escapeHTML(param.name) << "</code></td>\n"
                         << "      <td>" << paramTypeToString(param.type) << "</td>\n"
                         << "      <td class=\"" << (param.required ? "required" : "optional") << "\">"
                         << (param.required ? "Yes" : "No") << "</td>\n"
                         << "      <td><code>" << escapeHTML(getDefaultAsString(param.default_value)) << "</code></td>\n"
                         << "      <td>" << escapeHTML(param.description) << "</td>\n"
                         << "    </tr>\n";
                }

                html << "  </tbody>\n</table>\n";
            }
        }
    }

    html << "</body>\n</html>\n";

    return html.str();
}

std::string ConfigSchema::generateYAMLTemplate() const {
    std::ostringstream yaml;

    yaml << "# PIMID Configuration Template\n";
    yaml << "# Auto-generated from configuration schema\n";
    yaml << "# \n";
    yaml << "# Parameters marked [REQUIRED] must be specified\n";
    yaml << "# Parameters marked [OPTIONAL] have default values shown\n";
    yaml << "#\n\n";

    for (const auto& section : sections_) {
        yaml << "# " << std::string(70, '=') << "\n";
        yaml << "# " << section.name;
        if (section.required) yaml << " [REQUIRED SECTION]";
        yaml << "\n";
        if (!section.description.empty()) {
            yaml << "# " << section.description << "\n";
        }
        yaml << "# " << std::string(70, '=') << "\n";
        yaml << section.name << ":\n";

        // Generate parameters with comments
        for (const auto& param : section.parameters) {
            yaml << "  # " << param.description << "\n";
            yaml << "  # Type: " << paramTypeToString(param.type);
            if (param.required) {
                yaml << " [REQUIRED]";
            } else {
                yaml << " [OPTIONAL]";
            }
            yaml << "\n";

            if (!param.allowed_values.empty()) {
                yaml << "  # Allowed values: ";
                for (size_t i = 0; i < param.allowed_values.size(); ++i) {
                    if (i > 0) yaml << ", ";
                    yaml << param.allowed_values[i];
                }
                yaml << "\n";
            }

            // Print validation constraints
            for (const auto& rule : param.validation_rules) {
                switch (rule.type) {
                    case ValidationRule::RuleType::MIN_VALUE:
                        if (std::holds_alternative<int64_t>(rule.value)) {
                            yaml << "  # Minimum: " << std::get<int64_t>(rule.value) << "\n";
                        } else if (std::holds_alternative<double>(rule.value)) {
                            yaml << "  # Minimum: " << std::get<double>(rule.value) << "\n";
                        }
                        break;
                    case ValidationRule::RuleType::MAX_VALUE:
                        if (std::holds_alternative<int64_t>(rule.value)) {
                            yaml << "  # Maximum: " << std::get<int64_t>(rule.value) << "\n";
                        } else if (std::holds_alternative<double>(rule.value)) {
                            yaml << "  # Maximum: " << std::get<double>(rule.value) << "\n";
                        }
                        break;
                    default:
                        break;
                }
            }

            // Output the parameter with default value
            std::string default_str = getDefaultAsString(param.default_value);
            if (param.type == ParameterType::STRING ||
                param.type == ParameterType::ENUM ||
                param.type == ParameterType::FILE_PATH ||
                param.type == ParameterType::DIRECTORY_PATH) {
                yaml << "  " << param.name << ": \"" << default_str << "\"\n";
            } else if (param.type == ParameterType::LIST) {
                yaml << "  " << param.name << ":\n";
                if (!default_str.empty()) {
                    // Parse comma-separated default
                    std::istringstream iss(default_str);
                    std::string item;
                    while (std::getline(iss, item, ',')) {
                        yaml << "    - " << item << "\n";
                    }
                } else {
                    yaml << "    # - item1\n";
                    yaml << "    # - item2\n";
                }
            } else {
                yaml << "  " << param.name << ": " << default_str << "\n";
            }
            yaml << "\n";
        }

        // Generate subsections
        for (const auto& subsec : section.subsections) {
            yaml << "  # " << std::string(60, '-') << "\n";
            yaml << "  # " << subsec.name;
            if (subsec.required) yaml << " [REQUIRED]";
            yaml << "\n";
            if (!subsec.description.empty()) {
                yaml << "  # " << subsec.description << "\n";
            }
            yaml << "  # " << std::string(60, '-') << "\n";
            yaml << "  " << subsec.name << ":\n";

            for (const auto& param : subsec.parameters) {
                yaml << "    # " << param.description << "\n";
                yaml << "    # Type: " << paramTypeToString(param.type);
                yaml << (param.required ? " [REQUIRED]" : " [OPTIONAL]") << "\n";

                if (!param.allowed_values.empty()) {
                    yaml << "    # Allowed values: ";
                    for (size_t i = 0; i < param.allowed_values.size(); ++i) {
                        if (i > 0) yaml << ", ";
                        yaml << param.allowed_values[i];
                    }
                    yaml << "\n";
                }

                std::string default_str = getDefaultAsString(param.default_value);
                if (param.type == ParameterType::STRING ||
                    param.type == ParameterType::ENUM ||
                    param.type == ParameterType::FILE_PATH ||
                    param.type == ParameterType::DIRECTORY_PATH) {
                    yaml << "    " << param.name << ": \"" << default_str << "\"\n";
                } else {
                    yaml << "    " << param.name << ": " << default_str << "\n";
                }
                yaml << "\n";
            }
        }

        yaml << "\n";
    }

    return yaml.str();
}

std::string ConfigSchema::generateJSONSchema() const {
    std::ostringstream json;

    json << "{\n";
    json << "  \"$schema\": \"http://json-schema.org/draft-07/schema#\",\n";
    json << "  \"title\": \"PIMID Configuration Schema\",\n";
    json << "  \"description\": \"Configuration schema for the PIMID PIM simulator\",\n";
    json << "  \"type\": \"object\",\n";
    json << "  \"properties\": {\n";

    bool firstSection = true;
    for (const auto& section : sections_) {
        if (!firstSection) json << ",\n";
        firstSection = false;

        json << "    \"" << escapeJSON(section.name) << "\": {\n";
        json << "      \"type\": \"object\",\n";
        if (!section.description.empty()) {
            json << "      \"description\": \"" << escapeJSON(section.description) << "\",\n";
        }
        json << "      \"properties\": {\n";

        bool firstParam = true;
        for (const auto& param : section.parameters) {
            if (!firstParam) json << ",\n";
            firstParam = false;

            json << "        \"" << escapeJSON(param.name) << "\": {\n";
            json << "          \"type\": \"" << paramTypeToJSONSchemaType(param.type) << "\"";

            if (!param.description.empty()) {
                json << ",\n          \"description\": \"" << escapeJSON(param.description) << "\"";
            }

            // Default value
            std::string default_str = getDefaultAsString(param.default_value);
            if (!default_str.empty()) {
                if (param.type == ParameterType::STRING ||
                    param.type == ParameterType::ENUM ||
                    param.type == ParameterType::FILE_PATH ||
                    param.type == ParameterType::DIRECTORY_PATH) {
                    json << ",\n          \"default\": \"" << escapeJSON(default_str) << "\"";
                } else if (param.type == ParameterType::BOOLEAN) {
                    json << ",\n          \"default\": " << default_str;
                } else {
                    json << ",\n          \"default\": " << default_str;
                }
            }

            // Enum values
            if (!param.allowed_values.empty()) {
                json << ",\n          \"enum\": [";
                for (size_t i = 0; i < param.allowed_values.size(); ++i) {
                    if (i > 0) json << ", ";
                    json << "\"" << escapeJSON(param.allowed_values[i]) << "\"";
                }
                json << "]";
            }

            // Validation rules
            for (const auto& rule : param.validation_rules) {
                switch (rule.type) {
                    case ValidationRule::RuleType::MIN_VALUE:
                        if (std::holds_alternative<int64_t>(rule.value)) {
                            json << ",\n          \"minimum\": " << std::get<int64_t>(rule.value);
                        } else if (std::holds_alternative<double>(rule.value)) {
                            json << ",\n          \"minimum\": " << std::get<double>(rule.value);
                        }
                        break;
                    case ValidationRule::RuleType::MAX_VALUE:
                        if (std::holds_alternative<int64_t>(rule.value)) {
                            json << ",\n          \"maximum\": " << std::get<int64_t>(rule.value);
                        } else if (std::holds_alternative<double>(rule.value)) {
                            json << ",\n          \"maximum\": " << std::get<double>(rule.value);
                        }
                        break;
                    case ValidationRule::RuleType::REGEX:
                        if (std::holds_alternative<std::string>(rule.value)) {
                            json << ",\n          \"pattern\": \"" << escapeJSON(std::get<std::string>(rule.value)) << "\"";
                        }
                        break;
                    default:
                        break;
                }
            }

            json << "\n        }";
        }

        // Add subsection properties
        for (const auto& subsec : section.subsections) {
            if (!firstParam) json << ",\n";
            firstParam = false;

            json << "        \"" << escapeJSON(subsec.name) << "\": {\n";
            json << "          \"type\": \"object\",\n";
            if (!subsec.description.empty()) {
                json << "          \"description\": \"" << escapeJSON(subsec.description) << "\",\n";
            }
            json << "          \"properties\": {\n";

            bool firstSubParam = true;
            for (const auto& param : subsec.parameters) {
                if (!firstSubParam) json << ",\n";
                firstSubParam = false;

                json << "            \"" << escapeJSON(param.name) << "\": {\n";
                json << "              \"type\": \"" << paramTypeToJSONSchemaType(param.type) << "\"";

                if (!param.description.empty()) {
                    json << ",\n              \"description\": \"" << escapeJSON(param.description) << "\"";
                }

                std::string default_str = getDefaultAsString(param.default_value);
                if (!default_str.empty()) {
                    if (param.type == ParameterType::STRING ||
                        param.type == ParameterType::ENUM ||
                        param.type == ParameterType::FILE_PATH ||
                        param.type == ParameterType::DIRECTORY_PATH) {
                        json << ",\n              \"default\": \"" << escapeJSON(default_str) << "\"";
                    } else {
                        json << ",\n              \"default\": " << default_str;
                    }
                }

                if (!param.allowed_values.empty()) {
                    json << ",\n              \"enum\": [";
                    for (size_t i = 0; i < param.allowed_values.size(); ++i) {
                        if (i > 0) json << ", ";
                        json << "\"" << escapeJSON(param.allowed_values[i]) << "\"";
                    }
                    json << "]";
                }

                json << "\n            }";
            }

            json << "\n          }";

            // Required parameters in subsection
            std::vector<std::string> required_params;
            for (const auto& param : subsec.parameters) {
                if (param.required) {
                    required_params.push_back(param.name);
                }
            }
            if (!required_params.empty()) {
                json << ",\n          \"required\": [";
                for (size_t i = 0; i < required_params.size(); ++i) {
                    if (i > 0) json << ", ";
                    json << "\"" << escapeJSON(required_params[i]) << "\"";
                }
                json << "]";
            }

            json << "\n        }";
        }

        json << "\n      }";

        // Required parameters in section
        std::vector<std::string> required_params;
        for (const auto& param : section.parameters) {
            if (param.required) {
                required_params.push_back(param.name);
            }
        }
        for (const auto& subsec : section.subsections) {
            if (subsec.required) {
                required_params.push_back(subsec.name);
            }
        }
        if (!required_params.empty()) {
            json << ",\n      \"required\": [";
            for (size_t i = 0; i < required_params.size(); ++i) {
                if (i > 0) json << ", ";
                json << "\"" << escapeJSON(required_params[i]) << "\"";
            }
            json << "]";
        }

        json << "\n    }";
    }

    json << "\n  }";

    // Required sections at top level
    std::vector<std::string> required_sections;
    for (const auto& section : sections_) {
        if (section.required) {
            required_sections.push_back(section.name);
        }
    }
    if (!required_sections.empty()) {
        json << ",\n  \"required\": [";
        for (size_t i = 0; i < required_sections.size(); ++i) {
            if (i > 0) json << ", ";
            json << "\"" << escapeJSON(required_sections[i]) << "\"";
        }
        json << "]";
    }

    json << "\n}\n";

    return json.str();
}

void ConfigSchema::buildSectionMap() {
    section_map_.clear();
    for (auto& section : sections_) {
        section_map_[section.name] = &section;
    }
}

} // namespace config
} // namespace pimid
