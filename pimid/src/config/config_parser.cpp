#include "config/config_parser.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <sstream>

namespace pimid {
namespace config {

ConfigParser::ConfigParser() {}

bool ConfigParser::parseFile(const std::string& filename,
                              std::map<std::string, std::string>& config) {
    try {
        YAML::Node root = YAML::LoadFile(filename);
        return parseNode(root, "", config);
    } catch (const YAML::Exception& e) {
        std::cerr << "YAML parsing error in " << filename << ": "
                  << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Error reading file " << filename << ": "
                  << e.what() << std::endl;
        return false;
    }
}

bool ConfigParser::parseString(const std::string& yaml_content,
                                std::map<std::string, std::string>& config) {
    try {
        YAML::Node root = YAML::Load(yaml_content);
        return parseNode(root, "", config);
    } catch (const YAML::Exception& e) {
        std::cerr << "YAML parsing error: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigParser::parseNode(const YAML::Node& node,
                              const std::string& prefix,
                              std::map<std::string, std::string>& config) {
    if (!node.IsDefined() || node.IsNull()) {
        return true;
    }

    if (node.IsScalar()) {
        config[prefix] = node.as<std::string>();
        return true;
    }

    if (node.IsSequence()) {
        // Store list as comma-separated values
        std::ostringstream oss;
        for (size_t i = 0; i < node.size(); ++i) {
            if (i > 0) oss << ",";
            oss << node[i].as<std::string>();
        }
        config[prefix] = oss.str();
        return true;
    }

    if (node.IsMap()) {
        for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
            std::string key = it->first.as<std::string>();
            std::string full_key = prefix.empty() ? key : prefix + "." + key;

            if (!parseNode(it->second, full_key, config)) {
                return false;
            }
        }
        return true;
    }

    return true;
}

std::string ConfigParser::toYAML(const std::map<std::string, std::string>& config) {
    YAML::Emitter out;
    out << YAML::BeginMap;

    std::map<std::string, std::map<std::string, std::string>> grouped;

    // Group by top-level key
    for (const auto& kv : config) {
        size_t dot_pos = kv.first.find('.');
        if (dot_pos != std::string::npos) {
            std::string top_key = kv.first.substr(0, dot_pos);
            std::string sub_key = kv.first.substr(dot_pos + 1);
            grouped[top_key][sub_key] = kv.second;
        } else {
            grouped[kv.first][""] = kv.second;
        }
    }

    // Emit YAML
    for (const auto& group : grouped) {
        out << YAML::Key << group.first;
        if (group.second.size() == 1 && group.second.begin()->first.empty()) {
            out << YAML::Value << group.second.begin()->second;
        } else {
            out << YAML::Value << YAML::BeginMap;
            for (const auto& kv : group.second) {
                if (!kv.first.empty()) {
                    out << YAML::Key << kv.first << YAML::Value << kv.second;
                }
            }
            out << YAML::EndMap;
        }
    }

    out << YAML::EndMap;
    return std::string(out.c_str());
}

std::string ConfigParser::toJSON(const std::map<std::string, std::string>& config) {
    std::ostringstream oss;
    oss << "{\n";

    bool first = true;
    for (const auto& kv : config) {
        if (!first) oss << ",\n";
        first = false;
        oss << "  \"" << kv.first << "\": \"" << kv.second << "\"";
    }

    oss << "\n}";
    return oss.str();
}

std::vector<std::string> ConfigParser::parseList(const std::string& list_str) {
    std::vector<std::string> result;
    std::istringstream iss(list_str);
    std::string item;

    while (std::getline(iss, item, ',')) {
        // Trim whitespace
        size_t start = item.find_first_not_of(" \t\n\r");
        size_t end = item.find_last_not_of(" \t\n\r");

        if (start != std::string::npos && end != std::string::npos) {
            result.push_back(item.substr(start, end - start + 1));
        }
    }

    return result;
}

} // namespace config
} // namespace pimid
