#include "scheduler/scheduler.h"
#include <iostream>
#include <algorithm>
#include <limits>
#include <cmath>

namespace pimid {

//=============================================================================
// LoadBalancedScheduler Implementation
//=============================================================================

LoadBalancedScheduler::LoadBalancedScheduler(PEPlacementManager* pe_manager)
    : PEScheduler(SchedulingPolicy::LOAD_BALANCED, pe_manager) {
}

void LoadBalancedScheduler::submitTask(const PIMTask& task) {
    task_queue_.push(task);
}

uint32_t LoadBalancedScheduler::scheduleTask(const PIMTask& task) {
    // Select PE with least load
    uint32_t selected_pe = selectLeastLoadedPE();

    // Update statistics and load tracking
    stats_.total_tasks_scheduled++;
    stats_.tasks_per_pe[selected_pe]++;
    pe_total_tasks_[selected_pe]++;

    return selected_pe;
}

bool LoadBalancedScheduler::hasPendingTasks() const {
    return !task_queue_.empty();
}

PIMTask LoadBalancedScheduler::getNextTask() {
    if (task_queue_.empty()) {
        return PIMTask();
    }

    PIMTask task = task_queue_.front();
    task_queue_.pop();
    return task;
}

bool LoadBalancedScheduler::isPEAvailable(uint32_t pe_id) const {
    auto it = pe_busy_until_.find(pe_id);
    if (it == pe_busy_until_.end()) {
        return true;  // PE never used, so it's available
    }

    // Check if PE is still busy
    // In real implementation, would compare with simulation's current_cycle
    return true;
}

void LoadBalancedScheduler::markPEBusy(uint32_t pe_id, Cycle until_cycle) {
    pe_busy_until_[pe_id] = until_cycle;
}

void LoadBalancedScheduler::markPEIdle(uint32_t pe_id) {
    pe_busy_until_[pe_id] = 0;
}

PEScheduler::SchedulerStats LoadBalancedScheduler::getStats() const {
    SchedulerStats stats = stats_;

    // Calculate load balance factor (coefficient of variation)
    uint32_t total_pes = pe_manager_->getTotalPEs();
    if (total_pes > 0 && stats_.total_tasks_scheduled > 0) {
        double mean = stats_.total_tasks_scheduled / (double)total_pes;
        double variance = 0.0;

        for (uint32_t i = 0; i < total_pes; i++) {
            double diff = stats_.tasks_per_pe[i] - mean;
            variance += diff * diff;
        }
        variance /= total_pes;
        double std_dev = std::sqrt(variance);
        stats.load_balance_factor = std_dev / mean;  // Coefficient of variation
    }

    return stats;
}

void LoadBalancedScheduler::resetStats() {
    stats_ = SchedulerStats();
    pe_total_tasks_.clear();
}

void LoadBalancedScheduler::printStats() const {
    std::cout << "\n=== Load-Balanced Scheduler Statistics ===" << std::endl;
    std::cout << "Total Tasks Scheduled: " << stats_.total_tasks_scheduled << std::endl;

    // Show per-PE distribution
    std::cout << "\nTasks per PE:" << std::endl;
    uint32_t total_pes = pe_manager_->getTotalPEs();

    // Find min and max for visualization
    uint64_t min_tasks = UINT64_MAX;
    uint64_t max_tasks = 0;

    for (uint32_t i = 0; i < total_pes && i < 256; i++) {
        if (stats_.tasks_per_pe[i] > 0) {
            min_tasks = std::min(min_tasks, stats_.tasks_per_pe[i]);
            max_tasks = std::max(max_tasks, stats_.tasks_per_pe[i]);
        }
    }

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

            // Visual bar
            int bar_length = 20;
            if (max_tasks > 0) {
                int bars = (stats_.tasks_per_pe[i] * bar_length) / max_tasks;
                std::cout << " [";
                for (int b = 0; b < bars; b++) std::cout << "=";
                for (int b = bars; b < bar_length; b++) std::cout << " ";
                std::cout << "]";
            }

            std::cout << std::endl;
        }
    }

    // Calculate and display load balance metrics
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

        std::cout << "\nLoad Balance Metrics:" << std::endl;
        std::cout << "  Mean tasks/PE: " << mean << std::endl;
        std::cout << "  Std deviation: " << std_dev << std::endl;
        std::cout << "  Min tasks: " << min_tasks << std::endl;
        std::cout << "  Max tasks: " << max_tasks << std::endl;
        std::cout << "  Load imbalance: " << (max_tasks - min_tasks) << " tasks" << std::endl;
        std::cout << "  Coefficient of Variation: " << cv << std::endl;
        std::cout << "  (Lower is better, 0.0 = perfect balance)" << std::endl;
    }
}

uint32_t LoadBalancedScheduler::selectLeastLoadedPE() {
    uint32_t total_pes = pe_manager_->getTotalPEs();

    if (total_pes == 0) {
        std::cerr << "ERROR: No PEs registered with scheduler!" << std::endl;
        return 0;
    }

    // Find PE with minimum load
    uint32_t best_pe = 0;
    uint64_t min_load = UINT64_MAX;

    for (uint32_t pe_id = 0; pe_id < total_pes; pe_id++) {
        // Get current load for this PE
        uint64_t load = 0;
        auto it = pe_total_tasks_.find(pe_id);
        if (it != pe_total_tasks_.end()) {
            load = it->second;
        }

        // Consider both task count and busy status
        // If PE is busy, add penalty
        auto busy_it = pe_busy_until_.find(pe_id);
        if (busy_it != pe_busy_until_.end() && busy_it->second > 0) {
            load += 1;  // Small penalty for being busy
        }

        // Select PE with minimum load
        if (load < min_load) {
            min_load = load;
            best_pe = pe_id;
        }
    }

    return best_pe;
}

} // namespace pimid
