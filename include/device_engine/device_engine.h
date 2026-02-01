#ifndef PIMID_DEVICE_ENGINE_H
#define PIMID_DEVICE_ENGINE_H

#include "common/simulation_engine.h"
#include "communication/socket_comm.h"
#include "address_translation/pe_placement.h"
#include "address_translation/address_translator.h"
#include "scheduler/scheduler.h"
#include "network/network_model.h"
#include "execution_model/execution_model.h"
#include <memory>
#include <vector>

namespace pimid {

/**
 * Device simulation engine
 * Simulates memory-side processing elements and their interactions
 * Communicates with host engine for co-simulation
 */
class DeviceEngine : public SimulationEngine {
public:
    DeviceEngine(const PIMIDConfig& config, const std::string& host_addr, int port);
    ~DeviceEngine() override;

    // SimulationEngine interface implementation
    void initialize() override;
    void run(Cycle num_cycles) override;
    void finalize() override;

    // Device-specific operations
    void configurePEs(const MemoryHierarchy& hierarchy,
                      const std::vector<PEDescriptor>& pes);
    void configureMemory(MemoryTechnology tech, const std::string& config_path);
    void configureNetwork(const NetworkConfig& net_config);

    // Offload handling
    void handleOffloadRequest(Address code_addr, Address data_addr, uint64_t size);
    void completeOffload(uint32_t offload_id);

    // PE scheduling
    void setScheduler(std::unique_ptr<class PEScheduler> scheduler);
    uint32_t selectPE(Address data_addr);

private:
    // Communication with host
    std::unique_ptr<SocketComm> host_comm_;
    std::string host_address_;
    int comm_port_;
    bool host_connected_;

    // PE management
    std::unique_ptr<PEPlacementManager> pe_placement_;
    std::unique_ptr<PEScheduler> scheduler_;

    // Execution model (ZSim or Event-Driven) - NEW!
    std::shared_ptr<IExecutionModel> execution_model_;
    ExecutionModelType execution_model_type_;

    // Legacy ZSim integration for PEs (placeholder) - DEPRECATED, use execution_model_
    void* zsim_instance_;

    // Address translation
    std::unique_ptr<AddressTranslator> addr_translator_;

    // Offload tracking
    struct DeviceOffload {
        uint32_t offload_id;
        uint32_t assigned_pe;
        Address code_addr;
        Address data_addr;
        uint64_t data_size;
        Cycle start_cycle;
        Cycle completion_cycle;
        bool completed;
    };
    std::vector<DeviceOffload> active_offloads_;
    uint32_t next_offload_id_;

    // Internal methods
    void initializeZSim();
    void initializeCommunication();
    void handleHostMessages();
    void synchronizeWithHost();
    void executeOnPE(uint32_t pe_id, Address code_addr, Address data_addr);

    // Configuration parameters
    uint32_t total_num_pes_;
    Cycle offload_completion_cycles_;
    Cycle sync_interval_cycles_;

    // Statistics
    uint64_t total_offloads_handled_;
    Cycle total_execution_cycles_;
};

} // namespace pimid

#endif // PIMID_DEVICE_ENGINE_H
