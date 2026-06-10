#ifndef PIMID_TRACE_FORMAT_H
#define PIMID_TRACE_FORMAT_H

#include <cstdint>
#include <string>
#include <map>

namespace pimid {
namespace trace {

// Magic number: "PIMT" in ASCII
constexpr uint32_t TRACE_MAGIC = 0x50494D54;

// Version: 1.0 encoded as 0x0100
constexpr uint16_t TRACE_VERSION_MAJOR = 1;
constexpr uint16_t TRACE_VERSION_MINOR = 0;
constexpr uint16_t TRACE_VERSION = (TRACE_VERSION_MAJOR << 8) | TRACE_VERSION_MINOR;

/**
 * @brief Trace event types
 *
 * Events are categorized by which external model they feed into:
 * - Memory events -> Ramulator/CACTI/NVSim
 * - PIM events -> Ramulator with PIM payload
 * - Network events -> Garnet
 * - Compute events -> McPAT
 */
enum class TraceEventType : uint16_t {
    // Memory Events (feed to Ramulator/CACTI/NVSim)
    MEM_READ        = 0x0001,  // Read from memory
    MEM_WRITE       = 0x0002,  // Write to memory
    MEM_ATOMIC      = 0x0003,  // Atomic operation

    // PIM Events (feed to Ramulator with PIM payload)
    PIM_COMPUTE     = 0x0010,  // In-memory computation
    PIM_GATHER      = 0x0011,  // Multi-bank gather
    PIM_SCATTER     = 0x0012,  // Multi-bank scatter
    PIM_REDUCE      = 0x0013,  // Reduction operation

    // Network Events (feed to Garnet)
    NET_SEND        = 0x0020,  // Packet injection
    NET_RECV        = 0x0021,  // Packet extraction (for validation)

    // Compute Events (feed to McPAT)
    COMPUTE_INT     = 0x0030,  // Integer operations
    COMPUTE_FP      = 0x0031,  // Floating-point operations
    COMPUTE_VECTOR  = 0x0032,  // Vector/SIMD operations

    // Synchronization Events
    BARRIER         = 0x0040,  // PE barrier
    TASK_START      = 0x0041,  // Task begins
    TASK_END        = 0x0042,  // Task completes

    // OpenMP Events (0x0050-0x005F)
    OMP_PARALLEL_START = 0x0050,  // #pragma omp parallel begin
    OMP_PARALLEL_END   = 0x0051,  // #pragma omp parallel end
    OMP_BARRIER        = 0x0052,  // #pragma omp barrier
    OMP_CRITICAL_START = 0x0053,  // #pragma omp critical begin
    OMP_CRITICAL_END   = 0x0054,  // #pragma omp critical end
    OMP_ATOMIC         = 0x0055,  // #pragma omp atomic
    OMP_TASK_START     = 0x0056,  // #pragma omp task begin
    OMP_TASK_END       = 0x0057,  // #pragma omp task end
    OMP_TASKWAIT       = 0x0058,  // #pragma omp taskwait
    OMP_FLUSH          = 0x0059,  // #pragma omp flush

    // MPI Events (0x0060-0x006F)
    MPI_SEND           = 0x0060,  // MPI_Send
    MPI_RECV           = 0x0061,  // MPI_Recv
    MPI_ISEND          = 0x0062,  // MPI_Isend
    MPI_IRECV          = 0x0063,  // MPI_Irecv
    MPI_WAIT           = 0x0064,  // MPI_Wait
    MPI_BARRIER        = 0x0065,  // MPI_Barrier
    MPI_BCAST          = 0x0066,  // MPI_Bcast
    MPI_REDUCE         = 0x0067,  // MPI_Reduce
    MPI_ALLREDUCE      = 0x0068,  // MPI_Allreduce
    MPI_SCATTER        = 0x0069,  // MPI_Scatter
    MPI_GATHER         = 0x006A,  // MPI_Gather
    MPI_ALLGATHER      = 0x006B,  // MPI_Allgather
    MPI_ALLTOALL       = 0x006C,  // MPI_Alltoall

    // Co-Simulation Events (0x0070-0x007F)
    OFFLOAD_START      = 0x0070,  // Host offloads work to device
    OFFLOAD_END        = 0x0071,  // Device offload completes
};

/**
 * @brief Event flags (bit field)
 */
enum TraceEventFlags : uint16_t {
    FLAG_NONE           = 0x0000,
    FLAG_CACHE_HIT      = 0x0001,  // L1/L2 cache hit (for validation)
    FLAG_ROW_BUFFER_HIT = 0x0002,  // DRAM row buffer hit
    FLAG_PIM_LOCAL      = 0x0004,  // PIM operation is local
    FLAG_PIM_REMOTE     = 0x0008,  // PIM requires network
    FLAG_SPECULATIVE    = 0x0010,  // Speculative execution
    FLAG_DEVICE_DOMAIN  = 0x0020,  // Event from device (PIM) domain
};

/**
 * @brief Binary trace event structure (48 bytes, fixed size)
 *
 * Fixed-size events enable:
 * - Fast seeking to any event by index
 * - Memory-mapped I/O for efficient reading
 * - Simple random access patterns
 *
 * Fields are ordered to avoid alignment padding:
 * - 64-bit fields first (8-byte aligned)
 * - 32-bit fields next (4-byte aligned)
 * - 16-bit fields last (2-byte aligned)
 */
struct TraceEvent {
    // 64-bit fields (24 bytes)
    uint64_t cycle;           // 8 bytes - Event timestamp (simulation cycle)
    uint64_t address;         // 8 bytes - Memory address (if applicable)
    uint64_t aux_data;        // 8 bytes - Event-specific auxiliary data

    // 32-bit fields (20 bytes)
    uint32_t pe_id;           // 4 bytes - Processing element ID
    uint32_t size;            // 4 bytes - Data size in bytes
    uint32_t src_node;        // 4 bytes - Network source (if applicable)
    uint32_t dst_node;        // 4 bytes - Network destination
    uint32_t reserved;        // 4 bytes - Future use / alignment padding

    // 16-bit fields (4 bytes)
    uint16_t event_type;      // 2 bytes - TraceEventType enum value
    uint16_t flags;           // 2 bytes - TraceEventFlags bit field
};  // Total: 24 + 20 + 4 = 48 bytes

static_assert(sizeof(TraceEvent) == 48, "TraceEvent must be exactly 48 bytes");

/**
 * @brief Header flags
 */
enum TraceHeaderFlags : uint16_t {
    HEADER_FLAG_NONE           = 0x0000,
    HEADER_FLAG_COMPRESSED     = 0x0001,  // Events are compressed (future)
    HEADER_FLAG_HAS_INDEX      = 0x0002,  // File has event index table (future)
};

/**
 * @brief Binary header structure (fixed 64 bytes)
 *
 * The header is followed by YAML metadata text, then binary events.
 */
struct TraceHeader {
    uint32_t magic;           // 4 bytes - TRACE_MAGIC
    uint16_t version;         // 2 bytes - TRACE_VERSION
    uint16_t header_flags;    // 2 bytes - TraceHeaderFlags
    uint64_t num_events;      // 8 bytes - Total number of events
    uint64_t header_size;     // 8 bytes - Size of header + YAML metadata (bytes before binary events)
    uint64_t num_pes;         // 8 bytes - Number of processing elements
    uint64_t first_cycle;     // 8 bytes - First event cycle
    uint64_t last_cycle;      // 8 bytes - Last event cycle
    uint32_t reserved[4];     // 16 bytes - Future use / alignment
};  // Total: 64 bytes

static_assert(sizeof(TraceHeader) == 64, "TraceHeader must be exactly 64 bytes");

/**
 * @brief Trace configuration metadata (stored as YAML text after binary header)
 */
struct TraceConfig {
    // Generator info
    std::string generator;        // "pimid-zsim" or "pimid-trace-gen"
    std::string timestamp;        // ISO 8601 timestamp
    std::string workload;         // Workload name/path

    // Memory configuration
    std::string memory_technology;  // DRAM, SRAM, STT_MRAM, etc.
    uint32_t channels = 1;
    uint32_t ranks_per_channel = 1;
    uint32_t banks = 4;
    uint32_t subarrays_per_bank = 1;

    // Network configuration
    std::string noc_topology;       // MESH_2D, H_TREE, etc.
    uint32_t noc_rows = 1;
    uint32_t noc_cols = 1;

    // PE configuration
    uint32_t num_pes = 1;
    std::string pe_type;            // in_order_core, ooo_core, etc.
    double frequency_mhz = 1000.0;

    // Simulation info
    uint64_t num_events = 0;

    // User-defined metadata
    std::map<std::string, std::string> custom_metadata;
};

/**
 * @brief Get string representation of event type
 */
inline const char* eventTypeToString(TraceEventType type) {
    switch (type) {
        case TraceEventType::MEM_READ:       return "MEM_READ";
        case TraceEventType::MEM_WRITE:      return "MEM_WRITE";
        case TraceEventType::MEM_ATOMIC:     return "MEM_ATOMIC";
        case TraceEventType::PIM_COMPUTE:    return "PIM_COMPUTE";
        case TraceEventType::PIM_GATHER:     return "PIM_GATHER";
        case TraceEventType::PIM_SCATTER:    return "PIM_SCATTER";
        case TraceEventType::PIM_REDUCE:     return "PIM_REDUCE";
        case TraceEventType::NET_SEND:       return "NET_SEND";
        case TraceEventType::NET_RECV:       return "NET_RECV";
        case TraceEventType::COMPUTE_INT:    return "COMPUTE_INT";
        case TraceEventType::COMPUTE_FP:     return "COMPUTE_FP";
        case TraceEventType::COMPUTE_VECTOR: return "COMPUTE_VECTOR";
        case TraceEventType::BARRIER:        return "BARRIER";
        case TraceEventType::TASK_START:     return "TASK_START";
        case TraceEventType::TASK_END:       return "TASK_END";
        // OpenMP events
        case TraceEventType::OMP_PARALLEL_START: return "OMP_PARALLEL_START";
        case TraceEventType::OMP_PARALLEL_END:   return "OMP_PARALLEL_END";
        case TraceEventType::OMP_BARRIER:        return "OMP_BARRIER";
        case TraceEventType::OMP_CRITICAL_START: return "OMP_CRITICAL_START";
        case TraceEventType::OMP_CRITICAL_END:   return "OMP_CRITICAL_END";
        case TraceEventType::OMP_ATOMIC:         return "OMP_ATOMIC";
        case TraceEventType::OMP_TASK_START:     return "OMP_TASK_START";
        case TraceEventType::OMP_TASK_END:       return "OMP_TASK_END";
        case TraceEventType::OMP_TASKWAIT:       return "OMP_TASKWAIT";
        case TraceEventType::OMP_FLUSH:          return "OMP_FLUSH";
        // MPI events
        case TraceEventType::MPI_SEND:           return "MPI_SEND";
        case TraceEventType::MPI_RECV:           return "MPI_RECV";
        case TraceEventType::MPI_ISEND:          return "MPI_ISEND";
        case TraceEventType::MPI_IRECV:          return "MPI_IRECV";
        case TraceEventType::MPI_WAIT:           return "MPI_WAIT";
        case TraceEventType::MPI_BARRIER:        return "MPI_BARRIER";
        case TraceEventType::MPI_BCAST:          return "MPI_BCAST";
        case TraceEventType::MPI_REDUCE:         return "MPI_REDUCE";
        case TraceEventType::MPI_ALLREDUCE:      return "MPI_ALLREDUCE";
        case TraceEventType::MPI_SCATTER:        return "MPI_SCATTER";
        case TraceEventType::MPI_GATHER:         return "MPI_GATHER";
        case TraceEventType::MPI_ALLGATHER:      return "MPI_ALLGATHER";
        case TraceEventType::MPI_ALLTOALL:       return "MPI_ALLTOALL";
        // Co-sim events
        case TraceEventType::OFFLOAD_START:      return "OFFLOAD_START";
        case TraceEventType::OFFLOAD_END:        return "OFFLOAD_END";
        default:                                 return "UNKNOWN";
    }
}

/**
 * @brief Check if event is a memory event
 */
inline bool isMemoryEvent(TraceEventType type) {
    uint16_t t = static_cast<uint16_t>(type);
    return t >= 0x0001 && t <= 0x000F;
}

/**
 * @brief Check if event is a PIM event
 */
inline bool isPIMEvent(TraceEventType type) {
    uint16_t t = static_cast<uint16_t>(type);
    return t >= 0x0010 && t <= 0x001F;
}

/**
 * @brief Check if event is a network event
 */
inline bool isNetworkEvent(TraceEventType type) {
    uint16_t t = static_cast<uint16_t>(type);
    return t >= 0x0020 && t <= 0x002F;
}

/**
 * @brief Check if event is a compute event
 */
inline bool isComputeEvent(TraceEventType type) {
    uint16_t t = static_cast<uint16_t>(type);
    return t >= 0x0030 && t <= 0x003F;
}

/**
 * @brief Check if event is a synchronization event
 */
inline bool isSyncEvent(TraceEventType type) {
    uint16_t t = static_cast<uint16_t>(type);
    return t >= 0x0040 && t <= 0x004F;
}

/**
 * @brief Check if event is an OpenMP event
 */
inline bool isOpenMPEvent(TraceEventType type) {
    uint16_t t = static_cast<uint16_t>(type);
    return t >= 0x0050 && t <= 0x005F;
}

/**
 * @brief Check if event is an MPI event
 */
inline bool isMPIEvent(TraceEventType type) {
    uint16_t t = static_cast<uint16_t>(type);
    return t >= 0x0060 && t <= 0x006F;
}

/**
 * @brief Check if event is a parallel abstraction event (OpenMP or MPI)
 */
inline bool isParallelEvent(TraceEventType type) {
    return isOpenMPEvent(type) || isMPIEvent(type);
}

/**
 * @brief Check if event is a co-simulation event
 */
inline bool isCoSimEvent(TraceEventType type) {
    uint16_t t = static_cast<uint16_t>(type);
    return t >= 0x0070 && t <= 0x007F;
}

}  // namespace trace
}  // namespace pimid

#endif  // PIMID_TRACE_FORMAT_H
