#include "trace/trace_writer.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>

namespace pimid {
namespace trace {

TraceWriter::TraceWriter()
    : event_count_(0)
    , first_cycle_(UINT64_MAX)
    , last_cycle_(0)
    , header_offset_(0)
    , events_offset_(0)
    , buffer_size_(10000) {
    buffer_.reserve(buffer_size_);
}

TraceWriter::~TraceWriter() {
    if (isOpen()) {
        close();
    }
}

bool TraceWriter::open(const std::string& filename, const TraceConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (file_.is_open()) {
        std::cerr << "TraceWriter: Already open, close first" << std::endl;
        return false;
    }

    file_.open(filename, std::ios::binary | std::ios::out);
    if (!file_.is_open()) {
        std::cerr << "TraceWriter: Failed to open " << filename << std::endl;
        return false;
    }

    filename_ = filename;
    config_ = config;
    event_count_ = 0;
    first_cycle_ = UINT64_MAX;
    last_cycle_ = 0;
    buffer_.clear();

    // Write header and YAML metadata
    writeHeader();
    writeYAMLMetadata();

    // Record where binary events start
    events_offset_ = file_.tellp();

    return true;
}

void TraceWriter::writeEvent(const TraceEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!file_.is_open()) {
        return;
    }

    buffer_.push_back(event);

    // Track cycle range
    if (event.cycle < first_cycle_) {
        first_cycle_ = event.cycle;
    }
    if (event.cycle > last_cycle_) {
        last_cycle_ = event.cycle;
    }

    // Auto-flush when buffer is full
    if (buffer_.size() >= buffer_size_) {
        flushBuffer();
    }
}

void TraceWriter::close() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!file_.is_open()) {
        return;
    }

    // Flush remaining events
    flushBuffer();

    // Update header with final counts
    updateHeader();

    file_.close();
}

void TraceWriter::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        flushBuffer();
        file_.flush();
    }
}

void TraceWriter::setBufferSize(size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_size_ = std::max(size, size_t(1));
    buffer_.reserve(buffer_size_);
}

// -------------------------------------------------------------------------
// Convenience Methods
// -------------------------------------------------------------------------

void TraceWriter::recordMemoryAccess(Cycle cycle, uint32_t pe_id, uint64_t addr,
                                     uint32_t size, bool is_write,
                                     uint16_t flags) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.address = addr;
    event.pe_id = pe_id;
    event.size = size;
    event.event_type = static_cast<uint16_t>(
        is_write ? TraceEventType::MEM_WRITE : TraceEventType::MEM_READ);
    event.flags = flags;
    writeEvent(event);
}

void TraceWriter::recordAtomicOp(Cycle cycle, uint32_t pe_id, uint64_t addr,
                                 uint32_t size, uint16_t flags) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.address = addr;
    event.pe_id = pe_id;
    event.size = size;
    event.event_type = static_cast<uint16_t>(TraceEventType::MEM_ATOMIC);
    event.flags = flags;
    writeEvent(event);
}

void TraceWriter::recordPIMOp(Cycle cycle, uint32_t pe_id, uint64_t addr,
                              uint32_t size, TraceEventType op_type,
                              uint16_t flags) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.address = addr;
    event.pe_id = pe_id;
    event.size = size;
    event.event_type = static_cast<uint16_t>(op_type);
    event.flags = flags;
    writeEvent(event);
}

void TraceWriter::recordNetworkPacket(Cycle cycle, uint32_t src_node,
                                      uint32_t dst_node, uint32_t size,
                                      uint64_t addr) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.address = addr;
    event.size = size;
    event.event_type = static_cast<uint16_t>(TraceEventType::NET_SEND);
    event.src_node = src_node;
    event.dst_node = dst_node;
    writeEvent(event);
}

void TraceWriter::recordCompute(Cycle cycle, uint32_t pe_id,
                                TraceEventType op_type, uint64_t num_ops) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.event_type = static_cast<uint16_t>(op_type);
    event.aux_data = num_ops;
    writeEvent(event);
}

void TraceWriter::recordBarrier(Cycle cycle, uint32_t pe_id, uint64_t barrier_id) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.event_type = static_cast<uint16_t>(TraceEventType::BARRIER);
    event.aux_data = barrier_id;
    writeEvent(event);
}

void TraceWriter::recordTaskStart(Cycle cycle, uint32_t pe_id, uint64_t task_id) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.event_type = static_cast<uint16_t>(TraceEventType::TASK_START);
    event.aux_data = task_id;
    writeEvent(event);
}

void TraceWriter::recordTaskEnd(Cycle cycle, uint32_t pe_id, uint64_t task_id) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.event_type = static_cast<uint16_t>(TraceEventType::TASK_END);
    event.aux_data = task_id;
    writeEvent(event);
}

// -------------------------------------------------------------------------
// OpenMP Event Recording
// -------------------------------------------------------------------------

void TraceWriter::recordOMPParallelStart(Cycle cycle, uint32_t pe_id, uint32_t num_threads) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.event_type = static_cast<uint16_t>(TraceEventType::OMP_PARALLEL_START);
    event.aux_data = num_threads;
    writeEvent(event);
}

void TraceWriter::recordOMPParallelEnd(Cycle cycle, uint32_t pe_id) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.event_type = static_cast<uint16_t>(TraceEventType::OMP_PARALLEL_END);
    writeEvent(event);
}

void TraceWriter::recordOMPBarrier(Cycle cycle, uint32_t pe_id, uint64_t barrier_id) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.event_type = static_cast<uint16_t>(TraceEventType::OMP_BARRIER);
    event.aux_data = barrier_id;
    writeEvent(event);
}

void TraceWriter::recordOMPCriticalStart(Cycle cycle, uint32_t pe_id, uint32_t lock_id) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.event_type = static_cast<uint16_t>(TraceEventType::OMP_CRITICAL_START);
    event.aux_data = lock_id;
    writeEvent(event);
}

void TraceWriter::recordOMPCriticalEnd(Cycle cycle, uint32_t pe_id, uint32_t lock_id) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.event_type = static_cast<uint16_t>(TraceEventType::OMP_CRITICAL_END);
    event.aux_data = lock_id;
    writeEvent(event);
}

void TraceWriter::recordOMPAtomic(Cycle cycle, uint32_t pe_id, uint64_t addr, uint16_t op_type) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.address = addr;
    event.event_type = static_cast<uint16_t>(TraceEventType::OMP_ATOMIC);
    event.aux_data = op_type;
    writeEvent(event);
}

// -------------------------------------------------------------------------
// MPI Event Recording
// -------------------------------------------------------------------------

void TraceWriter::recordMPISend(Cycle cycle, uint32_t src_pe, uint32_t dst_pe,
                                uint32_t size, int tag) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = src_pe;
    event.src_node = src_pe;
    event.dst_node = dst_pe;
    event.size = size;
    event.event_type = static_cast<uint16_t>(TraceEventType::MPI_SEND);
    event.aux_data = static_cast<uint64_t>(tag);
    writeEvent(event);
}

void TraceWriter::recordMPIRecv(Cycle cycle, uint32_t src_pe, uint32_t dst_pe,
                                uint32_t size, int tag) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = dst_pe;
    event.src_node = src_pe;
    event.dst_node = dst_pe;
    event.size = size;
    event.event_type = static_cast<uint16_t>(TraceEventType::MPI_RECV);
    event.aux_data = static_cast<uint64_t>(tag);
    writeEvent(event);
}

void TraceWriter::recordMPIBarrier(Cycle cycle, uint32_t pe_id, uint32_t num_ranks) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.event_type = static_cast<uint16_t>(TraceEventType::MPI_BARRIER);
    event.aux_data = num_ranks;
    writeEvent(event);
}

void TraceWriter::recordMPIBcast(Cycle cycle, uint32_t pe_id, uint32_t root_pe, uint32_t size) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.src_node = root_pe;
    event.size = size;
    event.event_type = static_cast<uint16_t>(TraceEventType::MPI_BCAST);
    writeEvent(event);
}

void TraceWriter::recordMPIReduce(Cycle cycle, uint32_t pe_id, uint32_t root_pe, uint32_t size) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.dst_node = root_pe;
    event.size = size;
    event.event_type = static_cast<uint16_t>(TraceEventType::MPI_REDUCE);
    writeEvent(event);
}

void TraceWriter::recordMPIAllreduce(Cycle cycle, uint32_t pe_id, uint32_t size) {
    TraceEvent event = {};
    event.cycle = cycle;
    event.pe_id = pe_id;
    event.size = size;
    event.event_type = static_cast<uint16_t>(TraceEventType::MPI_ALLREDUCE);
    writeEvent(event);
}

// -------------------------------------------------------------------------
// Internal Helpers
// -------------------------------------------------------------------------

void TraceWriter::writeHeader() {
    header_offset_ = file_.tellp();

    TraceHeader header = {};
    header.magic = TRACE_MAGIC;
    header.version = TRACE_VERSION;
    header.header_flags = HEADER_FLAG_NONE;
    header.num_events = 0;  // Will be updated on close
    header.header_size = 0; // Will be updated after YAML
    header.num_pes = config_.num_pes;
    header.first_cycle = 0;
    header.last_cycle = 0;

    file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
}

void TraceWriter::writeYAMLMetadata() {
    std::string yaml = generateYAML();

    // Write YAML with delimiter
    file_ << yaml;
    file_ << "---\n";  // YAML document end marker
}

std::string TraceWriter::generateYAML() const {
    std::ostringstream ss;

    // Generate ISO 8601 timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&time);

    ss << "# PIMID Trace v" << TRACE_VERSION_MAJOR << "." << TRACE_VERSION_MINOR << "\n";
    ss << "version: " << TRACE_VERSION_MAJOR << "." << TRACE_VERSION_MINOR << "\n";
    ss << "generator: " << config_.generator << "\n";
    ss << "timestamp: " << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ") << "\n";

    if (!config_.workload.empty()) {
        ss << "workload: " << config_.workload << "\n";
    }

    ss << "num_events: " << config_.num_events << "\n";
    ss << "num_pes: " << config_.num_pes << "\n";

    // Memory configuration
    ss << "memory_config:\n";
    ss << "  technology: " << config_.memory_technology << "\n";
    ss << "  channels: " << config_.channels << "\n";
    ss << "  ranks_per_channel: " << config_.ranks_per_channel << "\n";
    ss << "  banks: " << config_.banks << "\n";
    ss << "  subarrays_per_bank: " << config_.subarrays_per_bank << "\n";

    // Network configuration
    ss << "network_config:\n";
    ss << "  topology: " << config_.noc_topology << "\n";
    ss << "  rows: " << config_.noc_rows << "\n";
    ss << "  cols: " << config_.noc_cols << "\n";

    // PE configuration
    ss << "pe_config:\n";
    ss << "  type: " << config_.pe_type << "\n";
    ss << "  frequency_mhz: " << config_.frequency_mhz << "\n";

    // Custom metadata
    if (!config_.custom_metadata.empty()) {
        ss << "custom:\n";
        for (const auto& [key, value] : config_.custom_metadata) {
            ss << "  " << key << ": " << value << "\n";
        }
    }

    return ss.str();
}

void TraceWriter::flushBuffer() {
    if (buffer_.empty()) {
        return;
    }

    // Write all buffered events
    for (const auto& event : buffer_) {
        file_.write(reinterpret_cast<const char*>(&event), sizeof(TraceEvent));
    }

    event_count_ += buffer_.size();
    buffer_.clear();
}

void TraceWriter::updateHeader() {
    // Seek back to header position
    file_.seekp(header_offset_);

    TraceHeader header = {};
    header.magic = TRACE_MAGIC;
    header.version = TRACE_VERSION;
    header.header_flags = HEADER_FLAG_NONE;
    header.num_events = event_count_;
    header.header_size = events_offset_;
    header.num_pes = config_.num_pes;
    header.first_cycle = first_cycle_;
    header.last_cycle = last_cycle_;

    file_.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Seek back to end
    file_.seekp(0, std::ios::end);
}

}  // namespace trace
}  // namespace pimid
