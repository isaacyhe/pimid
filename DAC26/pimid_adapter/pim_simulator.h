/**
 * @file pim_simulator.h
 * @brief PIMID-based PIM simulator adapter for DAC26 workloads
 *
 * This adapter wraps PIMID components to provide energy and timing
 * simulation for DAC26 benchmark workloads.
 *
 * Technology: 45nm
 * Frequency: 1GHz
 */

#ifndef DAC26_PIM_SIMULATOR_H
#define DAC26_PIM_SIMULATOR_H

#include <cstdint>
#include <string>
#include <memory>

namespace dac26 {

/**
 * PIM operation types
 */
enum class PIMOperation {
    LOCAL_READ,          // Read from local subarray
    LOCAL_WRITE,         // Write to local subarray
    REMOTE_READ,         // Read from remote subarray
    REMOTE_WRITE,        // Write to remote subarray
    COMPUTE_INT,         // Integer computation
    COMPUTE_FP,          // Floating-point computation
    ATOMIC_OP,           // Atomic operation
    BARRIER_SYNC         // Synchronization barrier
};

/**
 * Interconnect topology
 */
enum class Topology {
    HTREE_BASELINE,      // H-tree baseline (through bank port/peripheral)
    LIBCOM               // Library communication (direct interconnect)
};

/**
 * Simulation results
 */
struct SimulationResults {
    uint64_t total_cycles;
    uint64_t compute_cycles;
    uint64_t memory_cycles;
    uint64_t network_cycles;

    uint64_t local_reads;
    uint64_t local_writes;
    uint64_t remote_reads;
    uint64_t remote_writes;
    uint64_t compute_ops;

    double total_energy_pJ;
    double compute_energy_pJ;
    double memory_energy_pJ;
    double network_energy_pJ;

    double execution_time_ns;

    SimulationResults() : total_cycles(0), compute_cycles(0), memory_cycles(0),
                          network_cycles(0), local_reads(0), local_writes(0),
                          remote_reads(0), remote_writes(0), compute_ops(0),
                          total_energy_pJ(0.0), compute_energy_pJ(0.0),
                          memory_energy_pJ(0.0), network_energy_pJ(0.0),
                          execution_time_ns(0.0) {}
};

/**
 * PIM Simulator Configuration
 */
struct PIMConfig {
    // Technology parameters
    uint32_t tech_node_nm = 45;          // 45nm technology
    double frequency_ghz = 1.0;           // 1GHz operating frequency
    double temperature_k = 350.0;         // Operating temperature

    // Architecture parameters
    uint32_t num_subarrays = 8;
    uint64_t subarray_size_kb = 4;        // 4KB per subarray
    uint32_t word_size_bits = 32;

    // Interconnect configuration
    Topology topology = Topology::HTREE_BASELINE;

    // Timing parameters (will be computed from PIMID models)
    uint32_t local_read_cycles = 0;       // Filled by PIMID
    uint32_t local_write_cycles = 0;      // Filled by PIMID
    uint32_t remote_access_cycles = 0;    // Filled by PIMID
    uint32_t compute_cycles = 0;          // Filled by PIMID

    // Energy parameters (will be computed from PIMID models)
    double local_read_energy_pJ = 0.0;    // Filled by PIMID
    double local_write_energy_pJ = 0.0;   // Filled by PIMID
    double remote_access_energy_pJ = 0.0; // Filled by PIMID
    double compute_energy_pJ = 0.0;       // Filled by PIMID
};

/**
 * PIM Simulator - Wraps PIMID components
 */
class PIMSimulator {
public:
    explicit PIMSimulator(const PIMConfig& config);
    ~PIMSimulator();

    // Initialize PIMID components
    void initialize();

    // Simulate operations
    void simulateOperation(PIMOperation op, uint64_t count = 1);
    void simulateMemoryAccess(bool is_local, bool is_read, uint64_t bytes);
    void simulateCompute(uint64_t ops);
    void simulateNetworkTransfer(uint32_t source_subarray, uint32_t dest_subarray, uint64_t bytes);

    // Get results
    const SimulationResults& getResults() const { return results_; }
    void resetStats();
    void printResults() const;

    // Configuration queries
    uint32_t getLocalReadLatency() const { return config_.local_read_cycles; }
    uint32_t getLocalWriteLatency() const { return config_.local_write_cycles; }
    uint32_t getRemoteAccessLatency() const { return config_.remote_access_cycles; }
    uint32_t getComputeLatency() const { return config_.compute_cycles; }

    double getLocalReadEnergy() const { return config_.local_read_energy_pJ; }
    double getLocalWriteEnergy() const { return config_.local_write_energy_pJ; }
    double getRemoteAccessEnergy() const { return config_.remote_access_energy_pJ; }
    double getComputeEnergy() const { return config_.compute_energy_pJ; }

private:
    PIMConfig config_;
    SimulationResults results_;

    // PIMID component instances (opaque pointers to avoid exposing PIMID headers)
    void* power_model_;
    void* memory_model_;

    // Helper functions
    void initializePowerModel();
    void initializeMemoryModel();
    void computeTimingParameters();
    void computeEnergyParameters();

    uint32_t computeHTreeLatency(uint32_t num_subarrays) const;
    double computeHTreeWireEnergy(uint32_t num_subarrays) const;
    double scaleTechnologyNode(double base_value_45nm) const;
    double scaleFrequency(double base_value_1ghz) const;
};

} // namespace dac26

#endif // DAC26_PIM_SIMULATOR_H
