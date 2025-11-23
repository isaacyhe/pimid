#ifndef PIMID_EXECUTION_MODEL_H
#define PIMID_EXECUTION_MODEL_H

#include "common/types.h"
#include "common/event_queue.h"
#include "memory_model.h"
#include <memory>
#include <string>
#include <functional>
#include <vector>

namespace pimid {

/**
 * @brief Execution model type enumeration
 */
enum class ExecutionModelType {
    ZSIM_EXECUTION_DRIVEN,    // Option 3: ZSim execution-driven simulation
    EVENT_DRIVEN_ANALYTICAL,  // Option 2: Event-driven analytical model
    TRACE_DRIVEN,             // Trace-based simulation
    HYBRID                    // Hybrid approach
};

/**
 * @brief Task descriptor for event-driven execution
 */
struct Task {
    uint64_t task_id;
    std::string kernel_name;

    // Data addresses
    std::vector<uint64_t> input_addresses;
    std::vector<uint64_t> output_addresses;
    uint64_t input_size;
    uint64_t output_size;

    // Operation counts for analytical modeling
    uint64_t num_ops;
    uint64_t num_loads;
    uint64_t num_stores;
    uint64_t num_flops;

    // Task dependencies
    std::vector<uint64_t> depends_on;

    // Execution parameters
    uint32_t pe_id;
    Cycle start_cycle;
    Cycle estimated_cycles;

    // User-defined parameters
    std::map<std::string, double> parameters;
};

/**
 * @brief Memory access descriptor
 */
struct MemoryAccess {
    uint64_t address;
    uint64_t size;
    bool is_read;
    Cycle issue_cycle;
    uint32_t core_id;

    enum class Pattern {
        SEQUENTIAL,
        STRIDED,
        RANDOM
    } pattern = Pattern::SEQUENTIAL;

    uint64_t stride = 0;
};

/**
 * @brief Execution statistics
 */
struct ExecutionStats {
    uint64_t total_instructions;
    uint64_t total_cycles;
    uint64_t total_tasks;
    uint64_t memory_accesses;
    double ipc;  // Instructions per cycle
    double utilization;
};

/**
 * @brief Base interface for execution models
 *
 * This abstraction allows PIMID to support multiple execution models:
 * - ZSim execution-driven (detailed instruction-level simulation)
 * - Event-driven analytical (fast task-based simulation)
 * - Trace-driven (replay recorded traces)
 * - Hybrid approaches
 */
class IExecutionModel {
public:
    virtual ~IExecutionModel() = default;

    // -------------------------------------------------------------------------
    // Lifecycle Management
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize the execution model
     * @param config_file Configuration file path
     * @param domain Simulation domain (HOST or DEVICE)
     * @return true on success
     */
    virtual bool initialize(const std::string& config_file,
                           SimulationDomain domain) = 0;

    /**
     * @brief Finalize and clean up
     */
    virtual void finalize() = 0;

    // -------------------------------------------------------------------------
    // Execution Interface
    // -------------------------------------------------------------------------

    /**
     * @brief Advance simulation by specified cycles
     * @param num_cycles Number of cycles to simulate
     */
    virtual void advanceCycles(Cycle num_cycles) = 0;

    /**
     * @brief Execute a single task (for event-driven models)
     * @param task Task descriptor
     * @return Completion cycle
     */
    virtual Cycle executeTask(const Task& task) = 0;

    /**
     * @brief Check if execution is idle
     */
    virtual bool isIdle() const = 0;

    // -------------------------------------------------------------------------
    // Memory Interface
    // -------------------------------------------------------------------------

    /**
     * @brief Register memory model for memory access simulation
     */
    virtual void registerMemoryModel(std::shared_ptr<MemoryModel> memory_model) = 0;

    /**
     * @brief Get memory access pattern for a task
     */
    virtual std::vector<MemoryAccess> getMemoryAccessPattern(const Task& task) const = 0;

    // -------------------------------------------------------------------------
    // Statistics and Monitoring
    // -------------------------------------------------------------------------

    /**
     * @brief Get execution statistics
     */
    virtual ExecutionStats getStats() const = 0;

    /**
     * @brief Reset statistics
     */
    virtual void resetStats() = 0;

    /**
     * @brief Get current cycle count
     */
    virtual Cycle getCurrentCycle() const = 0;

    /**
     * @brief Get model type
     */
    virtual ExecutionModelType getType() const = 0;

    /**
     * @brief Get model name
     */
    virtual std::string getName() const = 0;

    // -------------------------------------------------------------------------
    // Event Callbacks
    // -------------------------------------------------------------------------

    /**
     * @brief Register callback for task completion
     */
    virtual void registerTaskCompleteCallback(
        std::function<void(const Task&, Cycle)> callback) = 0;

    /**
     * @brief Register callback for memory operation
     */
    virtual void registerMemoryCallback(
        std::function<void(const MemoryAccess&, Cycle)> callback) = 0;
};

/**
 * @brief Factory for creating execution models
 */
class ExecutionModelFactory {
public:
    /**
     * @brief Create execution model based on type
     */
    static std::shared_ptr<IExecutionModel> createExecutionModel(
        ExecutionModelType type,
        const PIMIDConfig& config,
        SimulationDomain domain);

    /**
     * @brief Create execution model from configuration string
     */
    static std::shared_ptr<IExecutionModel> createFromConfig(
        const std::string& model_name,
        const PIMIDConfig& config,
        SimulationDomain domain);
};

} // namespace pimid

#endif // PIMID_EXECUTION_MODEL_H
