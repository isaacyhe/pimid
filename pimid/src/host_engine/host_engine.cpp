#include "host_engine/host_engine.h"
#include "communication/socket_comm.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <cstring>

namespace pimid {

HostEngine::HostEngine(const PIMIDConfig& config, int comm_port)
    : SimulationEngine(SimulationDomain::HOST, config),
      comm_port_(comm_port),
      device_connected_(false),
      zsim_instance_(nullptr),
      total_offloads_(0),
      total_offload_cycles_(0) {

    // Set default cache configuration
    cache_config_.l1i_size_kb = 32;
    cache_config_.l1d_size_kb = 32;
    cache_config_.l2_size_kb = 256;
    cache_config_.l3_size_kb = 8192;
    cache_config_.l1_line_size = 64;
    cache_config_.l1_associativity = 8;
    cache_config_.l2_associativity = 8;
    cache_config_.l3_associativity = 16;
}

HostEngine::~HostEngine() {
    finalize();
}

void HostEngine::initialize() {
    std::cout << "Initializing Host Engine..." << std::endl;

    // Initialize communication with device
    initializeCommunication();

    // Initialize address translator
    // addr_translator_ = std::make_unique<AddressTranslator>(config_);

    // Initialize ZSim (placeholder for now)
    initializeZSim();

    // Reset statistics
    stats_ = PIMIDStats();
    current_cycle_ = 0;

    std::cout << "Host Engine initialized successfully" << std::endl;
}

void HostEngine::run(Cycle num_cycles) {
    std::cout << "Host Engine running for " << num_cycles << " cycles..." << std::endl;

    Cycle target_cycle = current_cycle_ + num_cycles;

    while (current_cycle_ < target_cycle) {
        // Check for messages from device
        handleDeviceMessages();

        // Execute one cycle of simulation
        // In a real implementation, this would call ZSim's tick/advance
        // For now, just advance the cycle counter
        advanceCycle();

        // Periodically synchronize with device (every 1000 cycles)
        if (current_cycle_ % 1000 == 0) {
            synchronizeWithDevice();
        }

        stats_.total_cycles = current_cycle_;
    }

    std::cout << "Host Engine completed " << num_cycles << " cycles" << std::endl;
}

void HostEngine::finalize() {
    std::cout << "Finalizing Host Engine..." << std::endl;

    // Send terminate message to device
    if (device_connected_ && device_comm_) {
        CommMessage term_msg;
        term_msg.type = MessageType::TERMINATE;
        term_msg.timestamp = current_cycle_;
        term_msg.src_domain = SimulationDomain::HOST;
        term_msg.dst_domain = SimulationDomain::DEVICE;
        device_comm_->sendMessage(term_msg);
    }

    // Shutdown communication
    if (device_comm_) {
        device_comm_->shutdown();
    }

    // Print statistics
    printStats();

    std::cout << "Host Engine finalized" << std::endl;
}

void HostEngine::loadBinary(const std::string& binary_path) {
    std::cout << "Loading binary: " << binary_path << std::endl;
    // TODO: Implement binary loading via ZSim
    // For now, just store the path
}

void HostEngine::setArguments(int argc, char** argv) {
    std::cout << "Setting program arguments (argc=" << argc << ")" << std::endl;
    // TODO: Pass arguments to ZSim
}

void HostEngine::offloadToDevice(Address code_addr, Address data_addr, uint64_t data_size) {
    if (!device_connected_) {
        std::cerr << "Cannot offload: device not connected" << std::endl;
        return;
    }

    std::cout << "Offloading to device: code=0x" << std::hex << code_addr
              << " data=0x" << data_addr << std::dec
              << " size=" << data_size << std::endl;

    // Create offload request message
    CommMessage msg;
    msg.type = MessageType::OFFLOAD_REQUEST;
    msg.timestamp = current_cycle_;
    msg.src_domain = SimulationDomain::HOST;
    msg.dst_domain = SimulationDomain::DEVICE;
    msg.addr = code_addr;
    msg.size = data_size;
    msg.request_id = static_cast<uint32_t>(pending_offloads_.size());

    // Add data address as payload
    msg.data.resize(sizeof(Address));
    memcpy(msg.data.data(), &data_addr, sizeof(Address));

    // Send the message
    if (!device_comm_->sendMessage(msg)) {
        std::cerr << "Failed to send offload request" << std::endl;
        return;
    }

    // Track the offload request
    OffloadRequest req;
    req.code_addr = code_addr;
    req.data_addr = data_addr;
    req.data_size = data_size;
    req.start_cycle = current_cycle_;
    req.completed = false;
    pending_offloads_.push_back(req);

    total_offloads_++;
}

void HostEngine::waitForDeviceCompletion() {
    std::cout << "Waiting for device to complete offload..." << std::endl;

    // Wait for OFFLOAD_COMPLETE message
    while (true) {
        CommMessage msg;
        if (device_comm_->receiveMessage(msg)) {
            if (msg.type == MessageType::OFFLOAD_COMPLETE) {
                // Mark the corresponding offload as complete
                if (msg.request_id < pending_offloads_.size()) {
                    pending_offloads_[msg.request_id].completed = true;
                    Cycle elapsed = msg.timestamp - pending_offloads_[msg.request_id].start_cycle;
                    total_offload_cycles_ += elapsed;

                    std::cout << "Offload " << msg.request_id << " completed in "
                              << elapsed << " cycles" << std::endl;
                    break;
                }
            }
        } else {
            // No message available, sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void HostEngine::setCoreConfig(const std::vector<HostCoreConfig>& cores) {
    core_configs_ = cores;
    std::cout << "Configured " << cores.size() << " host cores" << std::endl;
}

void HostEngine::setCacheConfig(const HostCacheConfig& cache) {
    cache_config_ = cache;
    std::cout << "Configured host cache hierarchy:" << std::endl;
    std::cout << "  L1I: " << cache.l1i_size_kb << " KB" << std::endl;
    std::cout << "  L1D: " << cache.l1d_size_kb << " KB" << std::endl;
    std::cout << "  L2: " << cache.l2_size_kb << " KB" << std::endl;
    std::cout << "  L3: " << cache.l3_size_kb << " KB" << std::endl;
}

void HostEngine::initializeZSim() {
    std::cout << "Initializing ZSim for host simulation..." << std::endl;
    // TODO: Actual ZSim initialization
    // For now, just set to nullptr to indicate it's a placeholder
    zsim_instance_ = nullptr;
}

void HostEngine::initializeCommunication() {
    std::cout << "Initializing host communication on port " << comm_port_ << std::endl;

    // Create host communication channel (server)
    device_comm_ = CommFactory::createHostComm(comm_port_);

    if (!device_comm_->initialize()) {
        throw std::runtime_error("Failed to initialize host communication");
    }

    device_connected_ = true;
    std::cout << "Host communication established" << std::endl;
}

void HostEngine::handleDeviceMessages() {
    // Non-blocking check for messages
    while (device_comm_->hasMessage()) {
        CommMessage msg;
        if (!device_comm_->receiveMessage(msg)) {
            break;
        }

        switch (msg.type) {
            case MessageType::OFFLOAD_COMPLETE:
                // Mark offload as complete
                if (msg.request_id < pending_offloads_.size()) {
                    pending_offloads_[msg.request_id].completed = true;
                }
                break;

            case MessageType::MEMORY_REQUEST:
                // Handle cross-domain memory request
                std::cout << "Received memory request from device" << std::endl;
                // TODO: Process memory request and send response
                break;

            case MessageType::SYNC_REQUEST:
                // Send sync acknowledgment
                {
                    CommMessage ack;
                    ack.type = MessageType::SYNC_ACK;
                    ack.timestamp = current_cycle_;
                    ack.src_domain = SimulationDomain::HOST;
                    ack.dst_domain = SimulationDomain::DEVICE;
                    device_comm_->sendMessage(ack);
                }
                break;

            case MessageType::TERMINATE:
                device_connected_ = false;
                std::cout << "Received terminate signal from device" << std::endl;
                break;

            default:
                std::cerr << "Unknown message type: " << static_cast<int>(msg.type) << std::endl;
                break;
        }
    }
}

void HostEngine::synchronizeWithDevice() {
    if (!device_connected_) {
        return;
    }

    // Send sync request to device
    device_comm_->synchronize(current_cycle_);

    // Update remote cycle info
    Cycle remote_cycle = device_comm_->getRemoteCycle();

    // Could add logic here to handle cycle mismatches
    // For now, just continue
}

} // namespace pimid
