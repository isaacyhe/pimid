#ifndef PIMID_ZSIM_EXECUTION_MODEL_H
#define PIMID_ZSIM_EXECUTION_MODEL_H

#include "execution_model/execution_model.h"
#include <atomic>
#include <thread>
#include <mutex>

// Forward declarations for ZSim types
struct GlobSimInfo;
class Core;

namespace pimid {

/**
 * @brief ZSim-based execution-driven model (Option 3)
 *
 * This model integrates ZSim for detailed instruction-level simulation.
 * Based on MultiPIM and ramulator-pim integration patterns.
 *
 * Features:
 * - Detailed instruction execution via PIN instrumentation
 * - Cycle-accurate core modeling (OoO, simple, etc.)
 * - Cache hierarchy simulation
 * - Supports both host and device (PIM) cores
 *
 * References:
 * - MultiPIM: https://github.com/Systems-ShiftLab/MultiPIM
 * - Ramulator-PIM: https://github.com/CMU-SAFARI/ramulator-pim
 */
class ZSimExecutionModel : public IExecutionModel {
public:
    ZSimExecutionModel();
    virtual ~ZSimExecutionModel();

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
    ExecutionModelType getType() const override { return ExecutionModelType::ZSIM_EXECUTION_DRIVEN; }
    std::string getName() const override { return "ZSim Execution-Driven"; }

    void registerTaskCompleteCallback(
        std::function<void(const Task&, Cycle)> callback) override;
    void registerMemoryCallback(
        std::function<void(const MemoryAccess&, Cycle)> callback) override;

    // -------------------------------------------------------------------------
    // ZSim-Specific Interface
    // -------------------------------------------------------------------------

    /**
     * @brief Get ZSim global simulation info (for advanced users)
     */
    GlobSimInfo* getZSimGlobInfo() const { return zsim_glob_info_; }

    /**
     * @brief Get core by ID
     */
    Core* getCore(uint32_t core_id) const;

    /**
     * @brief Get number of cores
     */
    uint32_t getNumCores() const { return num_cores_; }

    /**
     * @brief Inject memory request from PIMID to ZSim
     * This allows external memory models (Ramulator) to interact with ZSim
     */
    void injectMemoryResponse(uint64_t address, Cycle latency);

    /**
     * @brief Configure for PIM mode
     * Based on MultiPIM's PIM configuration approach
     */
    void configurePIMMode(bool enable_pim, uint32_t num_pim_cores);

private:
    // ZSim instance
    GlobSimInfo* zsim_glob_info_;
    Core** cores_;
    uint32_t num_cores_;

    // Configuration
    SimulationDomain domain_;
    std::string config_file_;
    bool pim_mode_;

    // Memory model integration
    std::shared_ptr<MemoryModel> memory_model_;

    // Callbacks
    std::function<void(const Task&, Cycle)> task_complete_callback_;
    std::function<void(const MemoryAccess&, Cycle)> memory_callback_;

    // Statistics
    mutable std::mutex stats_mutex_;
    ExecutionStats stats_;

    // State
    std::atomic<Cycle> current_cycle_;
    std::atomic<bool> initialized_;
    std::atomic<bool> idle_;

    // Internal helpers
    void initializeZSim(const std::string& config_file);
    void setupMemoryInterception();
    void connectToRamulator();

    // ZSim callbacks (static methods called by ZSim)
    static void zsimMemoryRequestCallback(void* ctx, uint64_t address,
                                         uint64_t size, bool is_write);
    static void zsimCycleCallback(void* ctx, uint64_t cycle);
};

} // namespace pimid

#endif // PIMID_ZSIM_EXECUTION_MODEL_H
