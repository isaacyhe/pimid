#ifndef PIMID_TRACE_WRITER_H
#define PIMID_TRACE_WRITER_H

#include "trace/trace_format.h"
#include "common/types.h"
#include <fstream>
#include <memory>
#include <mutex>

namespace pimid {
namespace trace {

/**
 * @brief Trace file writer for recording simulation events
 *
 * The trace writer generates binary trace files that can be replayed
 * through the trace-driven execution model. The file format consists of:
 * 1. Binary header (64 bytes)
 * 2. YAML metadata (variable length text)
 * 3. Binary events (48 bytes each)
 *
 * Thread-safe: Multiple threads can call record methods concurrently.
 */
class TraceWriter {
public:
    TraceWriter();
    ~TraceWriter();

    // Non-copyable, non-movable (owns file handle)
    TraceWriter(const TraceWriter&) = delete;
    TraceWriter& operator=(const TraceWriter&) = delete;
    TraceWriter(TraceWriter&&) = delete;
    TraceWriter& operator=(TraceWriter&&) = delete;

    /**
     * @brief Open a trace file for writing
     * @param filename Output file path
     * @param config Trace configuration metadata
     * @return true on success, false on failure
     */
    bool open(const std::string& filename, const TraceConfig& config);

    /**
     * @brief Write a single event to the trace file
     * @param event The event to write
     *
     * Thread-safe. Events are buffered and flushed periodically.
     */
    void writeEvent(const TraceEvent& event);

    /**
     * @brief Close the trace file
     *
     * Flushes remaining events and updates the header with final event count.
     */
    void close();

    /**
     * @brief Check if the writer is open
     */
    bool isOpen() const { return file_.is_open(); }

    /**
     * @brief Get the number of events written
     */
    uint64_t getEventCount() const { return event_count_; }

    /**
     * @brief Get the output filename
     */
    const std::string& getFilename() const { return filename_; }

    // -------------------------------------------------------------------------
    // Convenience Methods
    // -------------------------------------------------------------------------

    /**
     * @brief Record a memory access event
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param addr Memory address
     * @param size Data size in bytes
     * @param is_write true for write, false for read
     * @param flags Optional event flags
     */
    void recordMemoryAccess(Cycle cycle, uint32_t pe_id, uint64_t addr,
                           uint32_t size, bool is_write,
                           uint16_t flags = FLAG_NONE);

    /**
     * @brief Record an atomic memory operation
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param addr Memory address
     * @param size Data size in bytes
     * @param flags Optional event flags
     */
    void recordAtomicOp(Cycle cycle, uint32_t pe_id, uint64_t addr,
                       uint32_t size, uint16_t flags = FLAG_NONE);

    /**
     * @brief Record a PIM compute operation
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param addr Memory address being computed on
     * @param size Data size in bytes
     * @param op_type PIM operation type (PIM_COMPUTE, PIM_GATHER, etc.)
     * @param flags Optional event flags
     */
    void recordPIMOp(Cycle cycle, uint32_t pe_id, uint64_t addr,
                    uint32_t size, TraceEventType op_type,
                    uint16_t flags = FLAG_NONE);

    /**
     * @brief Record a network packet transmission
     * @param cycle Simulation cycle
     * @param src_node Source node ID
     * @param dst_node Destination node ID
     * @param size Packet size in bytes
     * @param addr Associated memory address (if any)
     */
    void recordNetworkPacket(Cycle cycle, uint32_t src_node, uint32_t dst_node,
                            uint32_t size, uint64_t addr = 0);

    /**
     * @brief Record compute operations (for power modeling)
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param op_type Compute operation type (COMPUTE_INT, COMPUTE_FP, COMPUTE_VECTOR)
     * @param num_ops Number of operations
     */
    void recordCompute(Cycle cycle, uint32_t pe_id, TraceEventType op_type,
                      uint64_t num_ops);

    /**
     * @brief Record a barrier synchronization
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param barrier_id Barrier identifier
     */
    void recordBarrier(Cycle cycle, uint32_t pe_id, uint64_t barrier_id = 0);

    /**
     * @brief Record task start
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param task_id Task identifier
     */
    void recordTaskStart(Cycle cycle, uint32_t pe_id, uint64_t task_id);

    /**
     * @brief Record task end
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param task_id Task identifier
     */
    void recordTaskEnd(Cycle cycle, uint32_t pe_id, uint64_t task_id);

    // -------------------------------------------------------------------------
    // OpenMP Event Recording
    // -------------------------------------------------------------------------

    /**
     * @brief Record OpenMP parallel region start
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param num_threads Number of threads in region
     */
    void recordOMPParallelStart(Cycle cycle, uint32_t pe_id, uint32_t num_threads);

    /**
     * @brief Record OpenMP parallel region end
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     */
    void recordOMPParallelEnd(Cycle cycle, uint32_t pe_id);

    /**
     * @brief Record OpenMP barrier
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param barrier_id Barrier identifier
     */
    void recordOMPBarrier(Cycle cycle, uint32_t pe_id, uint64_t barrier_id = 0);

    /**
     * @brief Record OpenMP critical section start
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param lock_id Lock identifier
     */
    void recordOMPCriticalStart(Cycle cycle, uint32_t pe_id, uint32_t lock_id);

    /**
     * @brief Record OpenMP critical section end
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param lock_id Lock identifier
     */
    void recordOMPCriticalEnd(Cycle cycle, uint32_t pe_id, uint32_t lock_id);

    /**
     * @brief Record OpenMP atomic operation
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param addr Memory address
     * @param op_type Atomic operation subtype
     */
    void recordOMPAtomic(Cycle cycle, uint32_t pe_id, uint64_t addr, uint16_t op_type);

    // -------------------------------------------------------------------------
    // MPI Event Recording
    // -------------------------------------------------------------------------

    /**
     * @brief Record MPI send operation
     * @param cycle Simulation cycle
     * @param src_pe Source PE ID
     * @param dst_pe Destination PE ID
     * @param size Message size in bytes
     * @param tag Message tag
     */
    void recordMPISend(Cycle cycle, uint32_t src_pe, uint32_t dst_pe,
                      uint32_t size, int tag);

    /**
     * @brief Record MPI receive operation
     * @param cycle Simulation cycle
     * @param src_pe Source PE ID
     * @param dst_pe Destination PE ID
     * @param size Message size in bytes
     * @param tag Message tag
     */
    void recordMPIRecv(Cycle cycle, uint32_t src_pe, uint32_t dst_pe,
                      uint32_t size, int tag);

    /**
     * @brief Record MPI barrier
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param num_ranks Number of ranks in barrier
     */
    void recordMPIBarrier(Cycle cycle, uint32_t pe_id, uint32_t num_ranks);

    /**
     * @brief Record MPI broadcast
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param root_pe Root PE ID
     * @param size Message size in bytes
     */
    void recordMPIBcast(Cycle cycle, uint32_t pe_id, uint32_t root_pe, uint32_t size);

    /**
     * @brief Record MPI reduce
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param root_pe Root PE ID
     * @param size Data size in bytes
     */
    void recordMPIReduce(Cycle cycle, uint32_t pe_id, uint32_t root_pe, uint32_t size);

    /**
     * @brief Record MPI allreduce
     * @param cycle Simulation cycle
     * @param pe_id Processing element ID
     * @param size Data size in bytes
     */
    void recordMPIAllreduce(Cycle cycle, uint32_t pe_id, uint32_t size);

    // -------------------------------------------------------------------------
    // Flush Control
    // -------------------------------------------------------------------------

    /**
     * @brief Flush buffered events to disk
     */
    void flush();

    /**
     * @brief Set the buffer size (number of events before auto-flush)
     * @param size Buffer size in number of events (default: 10000)
     */
    void setBufferSize(size_t size);

private:
    std::ofstream file_;
    std::string filename_;
    TraceConfig config_;
    uint64_t event_count_;
    uint64_t first_cycle_;
    uint64_t last_cycle_;
    uint64_t header_offset_;      // File offset where header starts
    uint64_t events_offset_;      // File offset where binary events start

    // Buffering
    std::vector<TraceEvent> buffer_;
    size_t buffer_size_;
    mutable std::mutex mutex_;

    // Internal helpers
    void writeHeader();
    void writeYAMLMetadata();
    void flushBuffer();
    void updateHeader();
    std::string generateYAML() const;
};

}  // namespace trace
}  // namespace pimid

#endif  // PIMID_TRACE_WRITER_H
