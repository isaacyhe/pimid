#include "device_engine/device_engine.h"
#include "communication/socket_comm.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <cstring>

namespace pimid {

DeviceEngine::DeviceEngine(const PIMIDConfig& config,
                           const std::string& host_addr, int port)
    : SimulationEngine(SimulationDomain::DEVICE, config),
      host_address_(host_addr),
      comm_port_(port),
      host_connected_(false),
      zsim_instance_(nullptr),
      next_offload_id_(0),
      total_offloads_handled_(0),
      total_execution_cycles_(0) {
}

DeviceEngine::~DeviceEngine() {
    finalize();
}

void DeviceEngine::initialize() {
    std::cout << "Initializing Device Engine..." << std::endl;

    // Initialize communication with host
    initializeCommunication();

    // Initialize PE placement manager
    // pe_placement_ = std::make_unique<PEPlacementManager>(config_);

    // Initialize address translator
    // addr_translator_ = std::make_unique<AddressTranslator>(config_);

    // Initialize ZSim for PEs (placeholder for now)
    initializeZSim();

    // Reset statistics
    stats_ = PIMIDStats();
    current_cycle_ = 0;

    std::cout << "Device Engine initialized successfully" << std::endl;
}

void DeviceEngine::run(Cycle num_cycles) {
    std::cout << "Device Engine running for " << num_cycles << " cycles..." << std::endl;

    Cycle target_cycle = current_cycle_ + num_cycles;

    while (current_cycle_ < target_cycle) {
        // Check for messages from host
        handleHostMessages();

        // Execute one cycle of simulation
        // In a real implementation, this would call ZSim's tick/advance for all PEs
        // For now, just advance the cycle counter
        advanceCycle();

        // Process active offloads
        for (auto& offload : active_offloads_) {
            if (!offload.completed) {
                // Simulate offload execution
                // In a real implementation, this would execute on PEs
                // For now, just check if enough cycles have elapsed
                if (current_cycle_ - offload.start_cycle >= 100) {
                    offload.completion_cycle = current_cycle_;
                    offload.completed = true;
                    completeOffload(offload.offload_id);
                }
            }
        }

        // Periodically synchronize with host (every 1000 cycles)
        if (current_cycle_ % 1000 == 0) {
            synchronizeWithHost();
        }

        stats_.total_cycles = current_cycle_;
    }

    std::cout << "Device Engine completed " << num_cycles << " cycles" << std::endl;
}

void DeviceEngine::finalize() {
    std::cout << "Finalizing Device Engine..." << std::endl;

    // Shutdown communication
    if (host_comm_) {
        host_comm_->shutdown();
    }

    // Print statistics
    printStats();

    std::cout << "Device Engine finalized" << std::endl;
}

void DeviceEngine::configurePEs(const MemoryHierarchy& hierarchy,
                                const std::vector<PEDescriptor>& pes) {
    std::cout << "Configuring " << pes.size() << " processing elements" << std::endl;
    // TODO: Configure PEs via PEPlacementManager
}

void DeviceEngine::configureMemory(MemoryTechnology tech,
                                   const std::string& config_path) {
    std::cout << "Configuring memory technology: ";
    switch (tech) {
        case MemoryTechnology::DRAM:
            std::cout << "DRAM";
            break;
        case MemoryTechnology::SRAM:
            std::cout << "SRAM";
            break;
        case MemoryTechnology::STT_MRAM:
            std::cout << "STT-MRAM";
            break;
    }
    std::cout << " (config: " << config_path << ")" << std::endl;
    // TODO: Initialize memory model
}

void DeviceEngine::configureNetwork(const NetworkConfig& net_config) {
    std::cout << "Configuring network" << std::endl;
    // TODO: Initialize network model
}

void DeviceEngine::handleOffloadRequest(Address code_addr, Address data_addr,
                                        uint64_t size) {
    std::cout << "Handling offload request: code=0x" << std::hex << code_addr
              << " data=0x" << data_addr << std::dec
              << " size=" << size << std::endl;

    // Select PE for execution
    uint32_t selected_pe = selectPE(data_addr);

    // Create offload tracking structure
    DeviceOffload offload;
    offload.offload_id = next_offload_id_++;
    offload.assigned_pe = selected_pe;
    offload.code_addr = code_addr;
    offload.data_addr = data_addr;
    offload.data_size = size;
    offload.start_cycle = current_cycle_;
    offload.completion_cycle = 0;
    offload.completed = false;

    active_offloads_.push_back(offload);
    total_offloads_handled_++;

    // Execute on the selected PE
    executeOnPE(selected_pe, code_addr, data_addr);

    std::cout << "Offload " << offload.offload_id << " assigned to PE " << selected_pe << std::endl;
}

void DeviceEngine::completeOffload(uint32_t offload_id) {
    std::cout << "Completing offload " << offload_id << std::endl;

    // Find the offload
    for (const auto& offload : active_offloads_) {
        if (offload.offload_id == offload_id && offload.completed) {
            // Send completion message to host
            CommMessage msg;
            msg.type = MessageType::OFFLOAD_COMPLETE;
            msg.timestamp = offload.completion_cycle;
            msg.src_domain = SimulationDomain::DEVICE;
            msg.dst_domain = SimulationDomain::HOST;
            msg.request_id = offload_id;

            if (!host_comm_->sendMessage(msg)) {
                std::cerr << "Failed to send offload completion message" << std::endl;
            }

            Cycle execution_time = offload.completion_cycle - offload.start_cycle;
            total_execution_cycles_ += execution_time;

            std::cout << "Offload " << offload_id << " completed in "
                      << execution_time << " cycles" << std::endl;
            break;
        }
    }
}

void DeviceEngine::setScheduler(std::unique_ptr<class PEScheduler> scheduler) {
    scheduler_ = std::move(scheduler);
    std::cout << "PE scheduler configured" << std::endl;
}

uint32_t DeviceEngine::selectPE(Address data_addr) {
    if (scheduler_) {
        // TODO: Use scheduler to select PE
        // return scheduler_->scheduleTask(...);
    }

    // Simple default: select based on data address
    // In a real implementation, this would use PEPlacementManager
    return static_cast<uint32_t>(data_addr % 16); // Assume 16 PEs
}

void DeviceEngine::initializeZSim() {
    std::cout << "Initializing ZSim for device PEs..." << std::endl;
    // TODO: Actual ZSim initialization for PEs
    // For now, just set to nullptr to indicate it's a placeholder
    zsim_instance_ = nullptr;
}

void DeviceEngine::initializeCommunication() {
    std::cout << "Initializing device communication to " << host_address_
              << ":" << comm_port_ << std::endl;

    // Create device communication channel (client)
    host_comm_ = CommFactory::createDeviceComm(host_address_, comm_port_);

    if (!host_comm_->initialize()) {
        throw std::runtime_error("Failed to initialize device communication");
    }

    host_connected_ = true;
    std::cout << "Device communication established" << std::endl;
}

void DeviceEngine::handleHostMessages() {
    // Non-blocking check for messages
    while (host_comm_->hasMessage()) {
        CommMessage msg;
        if (!host_comm_->receiveMessage(msg)) {
            break;
        }

        switch (msg.type) {
            case MessageType::OFFLOAD_REQUEST:
                {
                    // Extract data address from payload
                    Address data_addr = 0;
                    if (msg.data.size() >= sizeof(Address)) {
                        memcpy(&data_addr, msg.data.data(), sizeof(Address));
                    }
                    handleOffloadRequest(msg.addr, data_addr, msg.size);
                }
                break;

            case MessageType::MEMORY_RESPONSE:
                // Handle memory response from host
                std::cout << "Received memory response from host" << std::endl;
                // TODO: Process memory response
                break;

            case MessageType::SYNC_REQUEST:
                // Send sync acknowledgment
                {
                    CommMessage ack;
                    ack.type = MessageType::SYNC_ACK;
                    ack.timestamp = current_cycle_;
                    ack.src_domain = SimulationDomain::DEVICE;
                    ack.dst_domain = SimulationDomain::HOST;
                    host_comm_->sendMessage(ack);
                }
                break;

            case MessageType::TERMINATE:
                host_connected_ = false;
                std::cout << "Received terminate signal from host" << std::endl;
                break;

            default:
                std::cerr << "Unknown message type: " << static_cast<int>(msg.type) << std::endl;
                break;
        }
    }
}

void DeviceEngine::synchronizeWithHost() {
    if (!host_connected_) {
        return;
    }

    // Send sync request to host
    host_comm_->synchronize(current_cycle_);

    // Update remote cycle info
    Cycle remote_cycle = host_comm_->getRemoteCycle();

    // Could add logic here to handle cycle mismatches
    // For now, just continue
}

void DeviceEngine::executeOnPE(uint32_t pe_id, Address code_addr, Address data_addr) {
    std::cout << "Executing on PE " << pe_id << ": code=0x" << std::hex
              << code_addr << " data=0x" << data_addr << std::dec << std::endl;

    // TODO: Actual execution on PE via ZSim
    // For now, this is a placeholder
    // The execution is simulated in the run() loop
}

} // namespace pimid
