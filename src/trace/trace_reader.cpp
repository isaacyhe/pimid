#include "trace/trace_reader.h"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace pimid {
namespace trace {

TraceReader::TraceReader()
    : current_index_(0)
    , events_offset_(0)
    , file_size_(0) {
}

TraceReader::~TraceReader() {
    if (isOpen()) {
        close();
    }
}

bool TraceReader::open(const std::string& filename) {
    if (file_.is_open()) {
        close();
    }

    file_.open(filename, std::ios::binary | std::ios::in);
    if (!file_.is_open()) {
        std::cerr << "TraceReader: Failed to open " << filename << std::endl;
        return false;
    }

    filename_ = filename;

    // Get file size
    file_.seekg(0, std::ios::end);
    file_size_ = file_.tellg();
    file_.seekg(0, std::ios::beg);

    // Read and validate header
    if (!readHeader()) {
        file_.close();
        return false;
    }

    // Set position to first event
    events_offset_ = header_.header_size;
    current_index_ = 0;
    file_.seekg(events_offset_);

    return true;
}

void TraceReader::close() {
    if (file_.is_open()) {
        file_.close();
    }
    filename_.clear();
    current_index_ = 0;
}

bool TraceReader::hasNextEvent() const {
    return file_.is_open() && current_index_ < header_.num_events;
}

TraceEvent TraceReader::readNextEvent() {
    if (!hasNextEvent()) {
        throw std::runtime_error("TraceReader: No more events");
    }

    TraceEvent event;
    file_.read(reinterpret_cast<char*>(&event), sizeof(TraceEvent));

    if (file_.gcount() != sizeof(TraceEvent)) {
        throw std::runtime_error("TraceReader: Failed to read event");
    }

    current_index_++;
    return event;
}

TraceEvent TraceReader::peekNextEvent() {
    if (!hasNextEvent()) {
        throw std::runtime_error("TraceReader: No more events");
    }

    auto pos = file_.tellg();
    TraceEvent event;
    file_.read(reinterpret_cast<char*>(&event), sizeof(TraceEvent));

    if (file_.gcount() != sizeof(TraceEvent)) {
        throw std::runtime_error("TraceReader: Failed to read event");
    }

    // Restore position
    file_.seekg(pos);
    return event;
}

std::vector<TraceEvent> TraceReader::readEvents(size_t count) {
    std::vector<TraceEvent> events;
    events.reserve(std::min(count, static_cast<size_t>(header_.num_events - current_index_)));

    while (hasNextEvent() && events.size() < count) {
        events.push_back(readNextEvent());
    }

    return events;
}

bool TraceReader::seekToEvent(uint64_t index) {
    if (index >= header_.num_events) {
        return false;
    }

    uint64_t offset = events_offset_ + (index * sizeof(TraceEvent));
    file_.seekg(offset);
    current_index_ = index;

    return file_.good();
}

bool TraceReader::seekToCycle(uint64_t cycle) {
    if (cycle > header_.last_cycle) {
        return false;
    }

    // Linear scan to find first event at or after cycle
    // (Could be optimized with binary search if events are strictly ordered)
    rewind();

    while (hasNextEvent()) {
        auto pos = file_.tellg();
        TraceEvent event = readNextEvent();

        if (event.cycle >= cycle) {
            // Found it, go back one event
            file_.seekg(pos);
            current_index_--;
            return true;
        }
    }

    return false;
}

void TraceReader::rewind() {
    if (file_.is_open()) {
        file_.seekg(events_offset_);
        current_index_ = 0;
    }
}

bool TraceReader::validate() const {
    if (!file_.is_open()) {
        return false;
    }

    // Check magic number
    if (header_.magic != TRACE_MAGIC) {
        std::cerr << "TraceReader: Invalid magic number" << std::endl;
        return false;
    }

    // Check version
    if ((header_.version >> 8) != TRACE_VERSION_MAJOR) {
        std::cerr << "TraceReader: Incompatible version "
                  << (header_.version >> 8) << "." << (header_.version & 0xFF)
                  << " (expected " << TRACE_VERSION_MAJOR << ".x)" << std::endl;
        return false;
    }

    // Check file size
    uint64_t expected = getExpectedFileSize();
    if (file_size_ < expected) {
        std::cerr << "TraceReader: File truncated (expected " << expected
                  << " bytes, got " << file_size_ << ")" << std::endl;
        return false;
    }

    return true;
}

// -------------------------------------------------------------------------
// Internal Helpers
// -------------------------------------------------------------------------

bool TraceReader::readHeader() {
    // Read binary header
    file_.read(reinterpret_cast<char*>(&header_), sizeof(TraceHeader));
    if (file_.gcount() != sizeof(TraceHeader)) {
        std::cerr << "TraceReader: Failed to read header" << std::endl;
        return false;
    }

    // Validate magic
    if (header_.magic != TRACE_MAGIC) {
        std::cerr << "TraceReader: Invalid magic number (not a PIMID trace file)"
                  << std::endl;
        return false;
    }

    // Check version compatibility
    uint16_t major = header_.version >> 8;
    if (major != TRACE_VERSION_MAJOR) {
        std::cerr << "TraceReader: Incompatible trace version "
                  << major << "." << (header_.version & 0xFF)
                  << " (this reader supports v" << TRACE_VERSION_MAJOR << ".x)"
                  << std::endl;
        return false;
    }

    // Read YAML metadata
    uint64_t yaml_size = header_.header_size - sizeof(TraceHeader);
    if (yaml_size > 0 && yaml_size < 1024 * 1024) {  // Sanity check: < 1MB
        std::string yaml_text(yaml_size, '\0');
        file_.read(&yaml_text[0], yaml_size);

        if (!parseYAMLMetadata(yaml_text)) {
            std::cerr << "TraceReader: Warning: Failed to parse YAML metadata"
                      << std::endl;
            // Non-fatal, continue with defaults
        }
    }

    return true;
}

bool TraceReader::parseYAMLMetadata(const std::string& yaml_text) {
    // Simple YAML parser for our known format
    // For full YAML support, use yaml-cpp

    std::istringstream ss(yaml_text);
    std::string line;
    std::string current_section;

    while (std::getline(ss, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line == "---") {
            continue;
        }

        // Find key-value separator
        size_t colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, colon);
        std::string value = (colon + 1 < line.length()) ? line.substr(colon + 1) : "";

        // Trim whitespace
        while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) {
            key.erase(0, 1);
        }
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
            key.pop_back();
        }
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
            value.erase(0, 1);
        }
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
            value.pop_back();
        }

        // Check for section header (no value, line starts without indent)
        if (value.empty() && line[0] != ' ' && line[0] != '\t') {
            current_section = key;
            continue;
        }

        // Parse values
        if (key == "version") {
            // Already validated from binary header
        } else if (key == "generator") {
            config_.generator = value;
        } else if (key == "timestamp") {
            config_.timestamp = value;
        } else if (key == "workload") {
            config_.workload = value;
        } else if (key == "num_events") {
            config_.num_events = std::stoull(value);
        } else if (key == "num_pes") {
            config_.num_pes = std::stoul(value);
        } else if (current_section == "memory_config") {
            if (key == "technology") config_.memory_technology = value;
            else if (key == "channels") config_.channels = std::stoul(value);
            else if (key == "ranks_per_channel") config_.ranks_per_channel = std::stoul(value);
            else if (key == "banks") config_.banks = std::stoul(value);
            else if (key == "subarrays_per_bank") config_.subarrays_per_bank = std::stoul(value);
        } else if (current_section == "network_config") {
            if (key == "topology") config_.noc_topology = value;
            else if (key == "rows") config_.noc_rows = std::stoul(value);
            else if (key == "cols") config_.noc_cols = std::stoul(value);
        } else if (current_section == "pe_config") {
            if (key == "type") config_.pe_type = value;
            else if (key == "frequency_mhz") config_.frequency_mhz = std::stod(value);
        } else if (current_section == "custom") {
            config_.custom_metadata[key] = value;
        }
    }

    return true;
}

TraceEvent TraceReader::readEventAt(uint64_t index) {
    if (index >= header_.num_events) {
        throw std::runtime_error("TraceReader: Event index out of range");
    }

    // Save current position
    auto saved_pos = file_.tellg();
    uint64_t saved_index = current_index_;

    // Seek to requested event
    uint64_t offset = events_offset_ + (index * sizeof(TraceEvent));
    file_.seekg(offset);

    TraceEvent event;
    file_.read(reinterpret_cast<char*>(&event), sizeof(TraceEvent));

    // Restore position
    file_.seekg(saved_pos);
    current_index_ = saved_index;

    return event;
}

}  // namespace trace
}  // namespace pimid
