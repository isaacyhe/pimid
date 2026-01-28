#ifndef PIMID_EVENT_DRIVEN_EXECUTION_MODEL_H
#define PIMID_EVENT_DRIVEN_EXECUTION_MODEL_H

#include "execution_model/execution_model.h"
#include "common/event_queue.h"
#include <queue>
#include <unordered_map>

namespace pimid {

/**
 * @brief Event-driven analytical execution model (Option 2)
 *
 * This model uses analytical performance models instead of detailed
 * instruction-level simulation. Much faster than execution-driven for
 * large-scale PIM systems with hundreds/thousands of cores.
 *
 * Features:
 * - Task-based execution (no instruction-level simulation)
 * - Analytical performance models (Roofline, Configurable IPC)
 * - Event-driven timing (skip idle cycles)
 * - Configurable core types via CoreType struct
 * - 100-1000x faster than execution-driven
 *
 * Use Cases:
 * - Large-scale design space exploration
 * - High-level architecture studies
 * - Fast iteration during early design phases
 * - Systems with 100s-1000s of PIM cores
 */
class EventDrivenExecutionModel : public IExecutionModel {
public:
    EventDrivenExecutionModel();
    virtual ~EventDrivenExecutionModel();

    // IExecutionModel interface implementation
    bool initialize(const std::string& config_file,
                   SimulationDomain domain) override;
    void finalize() override;
    void advanceCycles(Cycle num_cycles) override;
    Cycle executeTask(const Task& task) override;
    bool isIdle() const override;

    void registerMemoryModel(std::shared_ptr<MemoryModel> memory_model) override;
    std::vector<MemoryAccess> getMemoryAccessPattern(const Task& task) const override;

    ExecutionStats getStats() const override;
    void resetStats() override;
    Cycle getCurrentCycle() const override;
    ExecutionModelType getType() const override { return ExecutionModelType::EVENT_DRIVEN_ANALYTICAL; }
    std::string getName() const override { return "Event-Driven Analytical"; }

    void registerTaskCompleteCallback(
        std::function<void(const Task&, Cycle)> callback) override;
    void registerMemoryCallback(
        std::function<void(const MemoryAccess&, Cycle)> callback) override;

    // -------------------------------------------------------------------------
    // Event-Driven Specific Interface
    // -------------------------------------------------------------------------

    /**
     * @brief Core type configuration for analytical models
     */
    struct CoreType {
        double frequency_mhz;
        double ipc;              // Instructions per cycle
        uint32_t vector_width;   // SIMD width
        uint32_t pipeline_depth;
        std::string name;

        CoreType() : frequency_mhz(1000.0), ipc(1.0), vector_width(1),
                     pipeline_depth(5), name("GenericCore") {}
    };

    /**
     * @brief Set number of cores/PEs
     */
    void setNumCores(uint32_t num_cores);

    /**
     * @brief Set analytical performance model
     */
    enum class PerformanceModel {
        ROOFLINE,          // Roofline model (compute vs memory bound)
        CONFIGURABLE_IPC   // Configurable IPC from config file
    };
    void setPerformanceModel(PerformanceModel model);

    /**
     * @brief Set core type parameters
     */
    void setCoreType(const CoreType& type);

    /**
     * @brief Get event queue (for external event scheduling)
     */
    std::shared_ptr<EventQueue> getEventQueue() const { return event_queue_; }

private:
    // Configuration
    SimulationDomain domain_;
    std::string config_file_;
    uint32_t num_cores_;
    PerformanceModel perf_model_;

    // Event queue for discrete event simulation
    std::shared_ptr<EventQueue> event_queue_;

    // Memory model integration
    std::shared_ptr<MemoryModel> memory_model_;

    // Task management
    std::queue<Task> pending_tasks_;
    std::unordered_map<uint64_t, Task> active_tasks_;
    std::vector<bool> core_busy_;

    // Callbacks
    std::function<void(const Task&, Cycle)> task_complete_callback_;
    std::function<void(const MemoryAccess&, Cycle)> memory_callback_;

    // Statistics
    mutable std::mutex stats_mutex_;
    ExecutionStats stats_;

    // State
    Cycle current_cycle_;
    bool initialized_;

    // Core configuration
    CoreType core_type_;

    // Internal methods
    void loadConfiguration(const std::string& config_file);
    Cycle estimateTaskLatency(const Task& task) const;
    std::vector<MemoryAccess> generateMemoryAccesses(const Task& task) const;
    void scheduleTaskCompletion(const Task& task, Cycle completion_cycle);
    void processTaskCompletion(const Task& task);
    void processMemoryAccess(const MemoryAccess& access);

    // Analytical model implementations
    Cycle rooflineModel(const Task& task) const;
    Cycle configurableIPCModel(const Task& task) const;

    // Memory access pattern generators
    std::vector<MemoryAccess> generateSequentialAccesses(
        uint64_t base_addr, uint64_t size, bool is_read, Cycle start_cycle) const;
    std::vector<MemoryAccess> generateStridedAccesses(
        uint64_t base_addr, uint64_t size, uint64_t stride,
        bool is_read, Cycle start_cycle) const;
};

/**
 * @brief Hybrid execution model
 *
 * Combines execution-driven and event-driven models.
 * Example: ZSim for host, event-driven for device
 */
class HybridExecutionModel : public IExecutionModel {
public:
    HybridExecutionModel(
        std::shared_ptr<IExecutionModel> host_model,
        std::shared_ptr<IExecutionModel> device_model);
    virtual ~HybridExecutionModel();

    // Implementation delegates to appropriate sub-model
    bool initialize(const std::string& config_file,
                   SimulationDomain domain) override;
    void finalize() override;
    void advanceCycles(Cycle num_cycles) override;
    Cycle executeTask(const Task& task) override;
    bool isIdle() const override;

    void registerMemoryModel(std::shared_ptr<MemoryModel> memory_model) override;
    std::vector<MemoryAccess> getMemoryAccessPattern(const Task& task) const override;

    ExecutionStats getStats() const override;
    void resetStats() override;
    Cycle getCurrentCycle() const override;
    ExecutionModelType getType() const override { return ExecutionModelType::HYBRID; }
    std::string getName() const override { return "Hybrid"; }

    void registerTaskCompleteCallback(
        std::function<void(const Task&, Cycle)> callback) override;
    void registerMemoryCallback(
        std::function<void(const MemoryAccess&, Cycle)> callback) override;

private:
    std::shared_ptr<IExecutionModel> host_model_;
    std::shared_ptr<IExecutionModel> device_model_;
    SimulationDomain current_domain_;
};

} // namespace pimid

#endif // PIMID_EVENT_DRIVEN_EXECUTION_MODEL_H
