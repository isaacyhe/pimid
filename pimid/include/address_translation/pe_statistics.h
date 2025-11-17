#ifndef PIMID_PE_STATISTICS_H
#define PIMID_PE_STATISTICS_H

#include "common/types.h"
#include <map>
#include <string>
#include <vector>

namespace pimid {

/**
 * Per-PE runtime statistics
 */
struct PEStats {
    uint32_t pe_id;

    // Task execution statistics
    uint64_t tasks_completed;
    uint64_t tasks_failed;
    Cycle total_execution_cycles;
    Cycle idle_cycles;

    // Memory access statistics
    uint64_t local_accesses;
    uint64_t remote_accesses;
    uint64_t total_bytes_accessed;

    // Bandwidth utilization
    double avg_bandwidth_utilization;  // Percentage (0-100)
    double peak_bandwidth_utilization;
    uint64_t bandwidth_constrained_cycles;

    // Bus contention
    uint64_t bus_contention_events;
    Cycle total_contention_delay;

    // Energy consumption (if available)
    double total_energy_j;
    double compute_energy_j;
    double memory_energy_j;

    PEStats() : pe_id(0), tasks_completed(0), tasks_failed(0),
                total_execution_cycles(0), idle_cycles(0),
                local_accesses(0), remote_accesses(0),
                total_bytes_accessed(0), avg_bandwidth_utilization(0.0),
                peak_bandwidth_utilization(0.0),
                bandwidth_constrained_cycles(0),
                bus_contention_events(0), total_contention_delay(0),
                total_energy_j(0.0), compute_energy_j(0.0),
                memory_energy_j(0.0) {}
};

/**
 * System-wide PE statistics aggregation
 */
struct SystemPEStats {
    uint32_t total_pes;
    Cycle simulation_cycles;

    // Aggregate statistics
    uint64_t total_tasks_completed;
    uint64_t total_tasks_failed;
    uint64_t total_local_accesses;
    uint64_t total_remote_accesses;

    // Load balancing metrics
    double load_balance_factor;  // 0-1, where 1 is perfectly balanced
    uint32_t most_loaded_pe;
    uint32_t least_loaded_pe;

    // Average metrics
    double avg_pe_utilization;
    double avg_bandwidth_utilization;
    double avg_locality_ratio;  // local / (local + remote)

    // System-wide totals
    double total_energy_j;
    uint64_t total_bytes_transferred;

    SystemPEStats() : total_pes(0), simulation_cycles(0),
                      total_tasks_completed(0), total_tasks_failed(0),
                      total_local_accesses(0), total_remote_accesses(0),
                      load_balance_factor(0.0), most_loaded_pe(0),
                      least_loaded_pe(0), avg_pe_utilization(0.0),
                      avg_bandwidth_utilization(0.0), avg_locality_ratio(0.0),
                      total_energy_j(0.0), total_bytes_transferred(0) {}
};

/**
 * Centralized PE statistics tracking manager
 * Collects, aggregates, and reports PE performance statistics
 */
class PEStatisticsManager {
public:
    PEStatisticsManager();

    // PE registration
    void registerPE(uint32_t pe_id);
    void unregisterPE(uint32_t pe_id);

    // Statistics recording
    void recordTaskCompletion(uint32_t pe_id, Cycle execution_cycles);
    void recordTaskFailure(uint32_t pe_id);
    void recordLocalAccess(uint32_t pe_id, uint64_t bytes);
    void recordRemoteAccess(uint32_t pe_id, uint64_t bytes);
    void recordBandwidthUtilization(uint32_t pe_id, double utilization_percent);
    void recordBusContention(uint32_t pe_id, Cycle contention_delay);
    void recordEnergyConsumption(uint32_t pe_id, double compute_j, double memory_j);
    void recordIdleCycles(uint32_t pe_id, Cycle cycles);

    // Statistics queries
    const PEStats& getPEStats(uint32_t pe_id) const;
    SystemPEStats getSystemStats() const;

    // Statistics aggregation
    void updateSimulationCycle(Cycle current_cycle);
    void calculateAggregateStats();

    // Reset and clear
    void resetStats();
    void resetPEStats(uint32_t pe_id);

    // Reporting
    void printPEStats(uint32_t pe_id) const;
    void printSystemStats() const;
    void printDetailedReport() const;
    void exportToJSON(const std::string& filename) const;
    void exportToCSV(const std::string& filename) const;

    // Analysis
    std::vector<uint32_t> getTopLoadedPEs(uint32_t count) const;
    std::vector<uint32_t> getBottleneckPEs() const;  // PEs with high contention
    std::map<uint32_t, double> getLoadDistribution() const;
    double calculateLoadImbalance() const;

private:
    std::map<uint32_t, PEStats> pe_stats_;
    SystemPEStats system_stats_;
    Cycle current_cycle_;

    // Helper functions
    void updatePeakBandwidth(uint32_t pe_id, double utilization);
    double calculateUtilization(const PEStats& stats) const;
    double calculateLocalityRatio(const PEStats& stats) const;
};

} // namespace pimid

#endif // PIMID_PE_STATISTICS_H
