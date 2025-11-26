#include "config/config_schema.h"
#include <iostream>

namespace pimid {
namespace config {

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
    return "# Configuration Schema\n\nTODO: Generate full documentation\n";
}

std::string ConfigSchema::generateHTMLDocs() const {
    return "<html><body><h1>Configuration Schema</h1><p>TODO</p></body></html>";
}

std::string ConfigSchema::generateYAMLTemplate() const {
    return "# YAML Configuration Template\n# TODO: Generate template\n";
}

std::string ConfigSchema::generateJSONSchema() const {
    return "{\"schema\": \"TODO\"}";
}

void ConfigSchema::buildSectionMap() {
    section_map_.clear();
    for (auto& section : sections_) {
        section_map_[section.name] = &section;
    }
}

} // namespace config
} // namespace pimid
