#include "scheduler/scheduler.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace pimid {

//=============================================================================
// RoundRobinScheduler Implementation
//=============================================================================

RoundRobinScheduler::RoundRobinScheduler(PEPlacementManager* pe_manager)
    : PEScheduler(SchedulingPolicy::ROUND_ROBIN, pe_manager)
    , next_pe_index_(0) {
}

void RoundRobinScheduler::submitTask(const PIMTask& task) {
    task_queue_.push(task);
}

uint32_t RoundRobinScheduler::scheduleTask(const PIMTask& task) {
    // Select next PE in round-robin fashion
    uint32_t selected_pe = selectNextPE();

    // Update statistics
    stats_.total_tasks_scheduled++;
    stats_.tasks_per_pe[selected_pe]++;

    return selected_pe;
}

bool RoundRobinScheduler::hasPendingTasks() const {
    return !task_queue_.empty();
}

PIMTask RoundRobinScheduler::getNextTask() {
    if (task_queue_.empty()) {
        return PIMTask();
    }

    PIMTask task = task_queue_.front();
    task_queue_.pop();
    return task;
}

bool RoundRobinScheduler::isPEAvailable(uint32_t pe_id) const {
    auto it = pe_busy_until_.find(pe_id);
    if (it == pe_busy_until_.end()) {
        return true;  // PE never used, so it's available
    }

    // Check if PE is still busy (simple model: assume current cycle = 0 for now)
    // In real implementation, would compare with simulation's current_cycle
    return true;
}

void RoundRobinScheduler::markPEBusy(uint32_t pe_id, Cycle until_cycle) {
    pe_busy_until_[pe_id] = until_cycle;
}

void RoundRobinScheduler::markPEIdle(uint32_t pe_id) {
    pe_busy_until_[pe_id] = 0;
}

PEScheduler::SchedulerStats RoundRobinScheduler::getStats() const {
    return stats_;
}

void RoundRobinScheduler::resetStats() {
    stats_ = SchedulerStats();
}

void RoundRobinScheduler::printStats() const {
    std::cout << "\n=== Round-Robin Scheduler Statistics ===" << std::endl;
    std::cout << "Total Tasks Scheduled: " << stats_.total_tasks_scheduled << std::endl;

    // Show per-PE distribution
    std::cout << "\nTasks per PE:" << std::endl;
    uint32_t total_pes = pe_manager_->getTotalPEs();
    for (uint32_t i = 0; i < total_pes && i < 256; i++) {
        if (stats_.tasks_per_pe[i] > 0) {
            std::cout << "  PE " << i << ": " << stats_.tasks_per_pe[i]
                      << " tasks";

            // Show percentage
            if (stats_.total_tasks_scheduled > 0) {
                double percentage = (stats_.tasks_per_pe[i] * 100.0) /
                                   stats_.total_tasks_scheduled;
                std::cout << " (" << percentage << "%)";
            }
            std::cout << std::endl;
        }
    }

    // Calculate load balance factor (lower is better)
    // Standard deviation of task distribution
    if (total_pes > 0 && stats_.total_tasks_scheduled > 0) {
        double mean = stats_.total_tasks_scheduled / (double)total_pes;
        double variance = 0.0;

        for (uint32_t i = 0; i < total_pes; i++) {
            double diff = stats_.tasks_per_pe[i] - mean;
            variance += diff * diff;
        }
        variance /= total_pes;
        double std_dev = std::sqrt(variance);
        double cv = std_dev / mean;  // Coefficient of variation

        std::cout << "\nLoad Balance Factor (CV): " << cv << std::endl;
        std::cout << "(Lower is better, 0.0 = perfect balance)" << std::endl;
    }
}

uint32_t RoundRobinScheduler::selectNextPE() {
    uint32_t total_pes = pe_manager_->getTotalPEs();

    if (total_pes == 0) {
        std::cerr << "ERROR: No PEs registered with scheduler!" << std::endl;
        return 0;
    }

    // Simple round-robin: select next PE and wrap around
    uint32_t selected = next_pe_index_;
    next_pe_index_ = (next_pe_index_ + 1) % total_pes;

    return selected;
}

} // namespace pimid
