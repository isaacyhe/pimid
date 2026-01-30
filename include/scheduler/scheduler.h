#ifndef PIMID_SCHEDULER_H
#define PIMID_SCHEDULER_H

#include "common/types.h"
#include "address_translation/pe_placement.h"
#include <vector>
#include <queue>

namespace pimid {

/**
 * Task descriptor for PIM execution
 */
struct PIMTask {
    uint32_t task_id;
    Address code_addr;
    Address data_addr;
    uint64_t data_size;
    uint32_t priority;
    Cycle arrival_cycle;

    PIMTask() : task_id(0), code_addr(0), data_addr(0),
                data_size(0), priority(0), arrival_cycle(0) {}
};

/**
 * Scheduling policies
 */
enum class SchedulingPolicy {
    NEAREST_PE,         // Schedule to PE closest to data
    ROUND_ROBIN,        // Round-robin across PEs
    LOAD_BALANCED,      // Balance load across PEs
    PRIORITY_BASED,     // Based on task priority
    DATA_AWARE          // Consider data locality
};

/**
 * Abstract PE scheduler interface
 * Handles task assignment to processing elements
 */
class PEScheduler {
public:
    PEScheduler(SchedulingPolicy policy, PEPlacementManager* pe_manager);
    virtual ~PEScheduler() = default;

    // Task management
    virtual void submitTask(const PIMTask& task) = 0;
    virtual uint32_t scheduleTask(const PIMTask& task) = 0;  // Returns PE ID
    virtual bool hasPendingTasks() const = 0;
    virtual PIMTask getNextTask() = 0;

    // PE status
    virtual bool isPEAvailable(uint32_t pe_id) const = 0;
    virtual void markPEBusy(uint32_t pe_id, Cycle until_cycle) = 0;
    virtual void markPEIdle(uint32_t pe_id) = 0;

    // Statistics
    struct SchedulerStats {
        uint64_t total_tasks_scheduled;
        uint64_t tasks_per_pe[256];  // Assume max 256 PEs
        Cycle avg_wait_time;
        double load_balance_factor;

        SchedulerStats() : total_tasks_scheduled(0), avg_wait_time(0),
                           load_balance_factor(0.0) {
            for (int i = 0; i < 256; i++) tasks_per_pe[i] = 0;
        }
    };

    virtual SchedulerStats getStats() const = 0;
    virtual void resetStats() = 0;
    virtual void printStats() const = 0;

    SchedulingPolicy getPolicy() const { return policy_; }

protected:
    SchedulingPolicy policy_;
    PEPlacementManager* pe_manager_;
};

/**
 * Nearest PE scheduler
 * Schedules tasks to PEs closest to the data
 */
class NearestPEScheduler : public PEScheduler {
public:
    NearestPEScheduler(PEPlacementManager* pe_manager);

    void submitTask(const PIMTask& task) override;
    uint32_t scheduleTask(const PIMTask& task) override;
    bool hasPendingTasks() const override;
    PIMTask getNextTask() override;

    bool isPEAvailable(uint32_t pe_id) const override;
    void markPEBusy(uint32_t pe_id, Cycle until_cycle) override;
    void markPEIdle(uint32_t pe_id) override;

    SchedulerStats getStats() const override;
    void resetStats() override;
    void printStats() const override;

private:
    std::queue<PIMTask> task_queue_;
    std::map<uint32_t, Cycle> pe_busy_until_;
    SchedulerStats stats_;

    uint32_t findNearestPE(Address data_addr) const;
};

/**
 * Round-robin scheduler
 */
class RoundRobinScheduler : public PEScheduler {
public:
    RoundRobinScheduler(PEPlacementManager* pe_manager);

    void submitTask(const PIMTask& task) override;
    uint32_t scheduleTask(const PIMTask& task) override;
    bool hasPendingTasks() const override;
    PIMTask getNextTask() override;

    bool isPEAvailable(uint32_t pe_id) const override;
    void markPEBusy(uint32_t pe_id, Cycle until_cycle) override;
    void markPEIdle(uint32_t pe_id) override;

    SchedulerStats getStats() const override;
    void resetStats() override;
    void printStats() const override;

private:
    std::queue<PIMTask> task_queue_;
    std::map<uint32_t, Cycle> pe_busy_until_;
    SchedulerStats stats_;
    uint32_t next_pe_index_;

    uint32_t selectNextPE();
};

/**
 * Load-balanced scheduler
 * Distributes tasks to minimize PE utilization variance
 */
class LoadBalancedScheduler : public PEScheduler {
public:
    LoadBalancedScheduler(PEPlacementManager* pe_manager);

    void submitTask(const PIMTask& task) override;
    uint32_t scheduleTask(const PIMTask& task) override;
    bool hasPendingTasks() const override;
    PIMTask getNextTask() override;

    bool isPEAvailable(uint32_t pe_id) const override;
    void markPEBusy(uint32_t pe_id, Cycle until_cycle) override;
    void markPEIdle(uint32_t pe_id) override;

    SchedulerStats getStats() const override;
    void resetStats() override;
    void printStats() const override;

private:
    std::queue<PIMTask> task_queue_;
    std::map<uint32_t, Cycle> pe_busy_until_;
    std::map<uint32_t, uint64_t> pe_total_tasks_;
    SchedulerStats stats_;

    uint32_t selectLeastLoadedPE();
};

/**
 * Scheduler factory
 */
class SchedulerFactory {
public:
    static std::unique_ptr<PEScheduler> createScheduler(
        SchedulingPolicy policy,
        PEPlacementManager* pe_manager);
};

} // namespace pimid

#endif // PIMID_SCHEDULER_H
