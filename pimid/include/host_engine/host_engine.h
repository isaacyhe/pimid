#ifndef PIMID_HOST_ENGINE_H
#define PIMID_HOST_ENGINE_H

#include "common/simulation_engine.h"
#include "communication/socket_comm.h"
#include "address_translation/address_translator.h"
#include "execution_model/execution_model.h"
#include <memory>
#include <vector>

namespace pimid {

/**
 * Host core descriptor
 */
struct HostCoreConfig {
    uint32_t core_id;
    uint32_t num_threads;
    uint32_t frequency_mhz;
    std::string core_type;  // in-order, out-of-order
    uint32_t pipeline_depth;
    uint32_t issue_width;
};

/**
 * Host cache configuration
 */
struct HostCacheConfig {
    uint64_t l1i_size_kb;
    uint64_t l1d_size_kb;
    uint64_t l2_size_kb;
    uint64_t l3_size_kb;
    uint32_t l1_line_size;
    uint32_t l1_associativity;
    uint32_t l2_associativity;
    uint32_t l3_associativity;
};

/**
 * Host simulation engine
 * Simulates conventional processor with cache hierarchy
 * Communicates with device engine for PIM offloading
 */
class HostEngine : public SimulationEngine {
public:
    HostEngine(const PIMIDConfig& config, int comm_port);
    ~HostEngine() override;

    // SimulationEngine interface implementation
    void initialize() override;
    void run(Cycle num_cycles) override;
    void finalize() override;

    // Host-specific operations
    void loadBinary(const std::string& binary_path);
    void setArguments(int argc, char** argv);

    // PIM offloading interface
    void offloadToDevice(Address code_addr, Address data_addr, uint64_t data_size);
    void waitForDeviceCompletion();

    // Configuration
    void setCoreConfig(const std::vector<HostCoreConfig>& cores);
    void setCacheConfig(const HostCacheConfig& cache);

private:
    // Communication with device
    std::unique_ptr<SocketComm> device_comm_;
    int comm_port_;
    bool device_connected_;

    // Host configuration
    std::vector<HostCoreConfig> core_configs_;
    HostCacheConfig cache_config_;

    // Execution model (ZSim or Event-Driven) - NEW!
    std::shared_ptr<IExecutionModel> execution_model_;
    ExecutionModelType execution_model_type_;

    // Legacy ZSim integration (placeholder) - DEPRECATED, use execution_model_
    void* zsim_instance_;

    // Address translation
    std::unique_ptr<AddressTranslator> addr_translator_;

    // Offload tracking
    struct OffloadRequest {
        Address code_addr;
        Address data_addr;
        uint64_t data_size;
        Cycle start_cycle;
        bool completed;
    };
    std::vector<OffloadRequest> pending_offloads_;

    // Internal methods
    void initializeZSim();
    void initializeCommunication();
    void handleDeviceMessages();
    void synchronizeWithDevice();

    // Statistics
    uint64_t total_offloads_;
    Cycle total_offload_cycles_;
};

} // namespace pimid

#endif // PIMID_HOST_ENGINE_H
