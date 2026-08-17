#include "scheduler/scheduler.h"
#include <iostream>
#include <algorithm>
#include <limits>
#include <cmath>

namespace pimid {

//=============================================================================
// NearestPEScheduler Implementation (Data-Locality-Aware Scheduling)
//=============================================================================

NearestPEScheduler::NearestPEScheduler(PEPlacementManager* pe_manager)
    : PEScheduler(SchedulingPolicy::NEAREST_PE, pe_manager) {
}

void NearestPEScheduler::submitTask(const PIMTask& task) {
    task_queue_.push(task);
}

uint32_t NearestPEScheduler::scheduleTask(const PIMTask& task) {
    // Find PE nearest to the data address
    uint32_t selected_pe = findNearestPE(task.data_addr);

    // Update statistics
    stats_.total_tasks_scheduled++;
    stats_.tasks_per_pe[selected_pe]++;

    return selected_pe;
}

bool NearestPEScheduler::hasPendingTasks() const {
    return !task_queue_.empty();
}

PIMTask NearestPEScheduler::getNextTask() {
    if (task_queue_.empty()) {
        return PIMTask();
    }

    PIMTask task = task_queue_.front();
    task_queue_.pop();
    return task;
}

bool NearestPEScheduler::isPEAvailable(uint32_t pe_id) const {
    auto it = pe_busy_until_.find(pe_id);
    if (it == pe_busy_until_.end()) {
        return true;  // PE never used, so it's available
    }

    // Check if PE is still busy
    // In real implementation, would compare with simulation's current_cycle
    return true;
}

void NearestPEScheduler::markPEBusy(uint32_t pe_id, Cycle until_cycle) {
    pe_busy_until_[pe_id] = until_cycle;
}

void NearestPEScheduler::markPEIdle(uint32_t pe_id) {
    pe_busy_until_[pe_id] = 0;
}

PEScheduler::SchedulerStats NearestPEScheduler::getStats() const {
    return stats_;
}

void NearestPEScheduler::resetStats() {
    stats_ = SchedulerStats();
}

void NearestPEScheduler::printStats() const {
    std::cout << "\n=== Nearest-PE (Data-Locality-Aware) Scheduler Statistics ===" << std::endl;
    std::cout << "Total Tasks Scheduled: " << stats_.total_tasks_scheduled << std::endl;

    // Show per-PE distribution
    std::cout << "\nTasks per PE:" << std::endl;
    uint32_t total_pes = pe_manager_->getTotalPEs();

    for (uint32_t i = 0; i < total_pes; i++) {
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

    // Calculate load balance factor
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
        std::cout << "(Note: May be higher than round-robin due to locality optimization)" << std::endl;
    }

    std::cout << "\n[at] Data Locality Optimization:" << std::endl;
    std::cout << "This scheduler prioritizes data locality over load balance." << std::endl;
    std::cout << "Tasks are assigned to PEs closest to their data to minimize" << std::endl;
    std::cout << "remote access penalties and maximize performance." << std::endl;
}

uint32_t NearestPEScheduler::findNearestPE(Address data_addr) const {
    uint32_t total_pes = pe_manager_->getTotalPEs();

    if (total_pes == 0) {
        std::cerr << "ERROR: No PEs registered with scheduler!" << std::endl;
        return 0;
    }

    // Strategy: Find PE with minimum access penalty for this address
    // Access penalty represents distance (local=0, remote=50-200 cycles)

    uint32_t best_pe = 0;
    uint32_t min_penalty = UINT32_MAX;
    bool found_local = false;

    for (uint32_t pe_id = 0; pe_id < total_pes; pe_id++) {
        // Check if PE can access this address
        if (!pe_manager_->canAccess(pe_id, data_addr)) {
            continue;  // Skip PEs that cannot access this address
        }

        // Get access penalty (0 for local, >0 for remote)
        uint32_t penalty = pe_manager_->getAccessPenalty(pe_id, data_addr);

        // Check if this is a local access
        bool is_local = pe_manager_->isLocalAddress(pe_id, data_addr);

        // Prioritize local accesses
        if (is_local && !found_local) {
            // First local PE found
            best_pe = pe_id;
            min_penalty = penalty;
            found_local = true;
        } else if (found_local && is_local) {
            // Multiple local PEs available, choose one with lower penalty
            // (e.g., if one is busy or has other considerations)
            if (penalty < min_penalty) {
                best_pe = pe_id;
                min_penalty = penalty;
            }
        } else if (!found_local) {
            // No local PE found yet, compare remote penalties
            if (penalty < min_penalty) {
                best_pe = pe_id;
                min_penalty = penalty;
            }
        }
    }

    // Log the scheduling decision (useful for debugging)
    if (min_penalty == 0) {
        // Local access - optimal!
    } else if (min_penalty < 100) {
        // Nearby access (e.g., same bank group or chip)
    } else {
        // Remote access (e.g., different rank)
        std::cerr << "WARNING: Task scheduled to remote PE " << best_pe
                  << " with penalty " << min_penalty << " cycles" << std::endl;
    }

    return best_pe;
}

} // namespace pimid
