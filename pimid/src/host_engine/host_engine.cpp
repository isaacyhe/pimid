#include "host_engine/host_engine.h"
#include "communication/socket_comm.h"
#include "config/config_manager.h"
#include "execution_model/zsim_execution_model.h"
#include "execution_model/event_driven_execution_model.h"
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
      execution_model_type_(ExecutionModelType::EVENT_DRIVEN_ANALYTICAL),
      zsim_instance_(nullptr),
      total_offloads_(0),
      total_offload_cycles_(0) {

    // Load cache configuration from ConfigManager
    auto& cfg = pimid::config::ConfigManager::getInstance();

    // Load cache sizes from configuration (configured in host.caches.* section)
    cache_config_.l1i_size_kb = static_cast<uint64_t>(
        cfg.getInt("host.caches.l1i.size_kb", 32));
    cache_config_.l1d_size_kb = static_cast<uint64_t>(
        cfg.getInt("host.caches.l1d.size_kb", 32));
    cache_config_.l2_size_kb = static_cast<uint64_t>(
        cfg.getInt("host.caches.l2.size_kb", 256));
    cache_config_.l3_size_kb = static_cast<uint64_t>(
        cfg.getInt("host.caches.l3.size_kb", 8192));

    // Load cache parameters
    cache_config_.l1_line_size = static_cast<uint32_t>(
        cfg.getInt("host.caches.l1i.line_size_bytes", 64));
    cache_config_.l1_associativity = static_cast<uint32_t>(
        cfg.getInt("host.caches.l1i.associativity", 8));
    cache_config_.l2_associativity = static_cast<uint32_t>(
        cfg.getInt("host.caches.l2.associativity", 8));
    cache_config_.l3_associativity = static_cast<uint32_t>(
        cfg.getInt("host.caches.l3.associativity", 16));

    // Load execution model type from configuration
    // Options: "zsim" | "execution_driven" | "analytical" | "event_driven"
    // Note: "hybrid" is deprecated - host and device are now configured independently
    std::string exec_model_name = cfg.getString("host.execution_model", "analytical");
    if (exec_model_name == "zsim" || exec_model_name == "execution_driven") {
        execution_model_type_ = ExecutionModelType::ZSIM_EXECUTION_DRIVEN;
    } else {
        // Default to analytical (fast, event-driven)
        // "hybrid" and "event_driven" both map to analytical
        execution_model_type_ = ExecutionModelType::EVENT_DRIVEN_ANALYTICAL;
    }

    std::cout << "Host Engine cache configuration loaded from config" << std::endl;
    std::cout << "  L1I: " << cache_config_.l1i_size_kb << " KB, "
              << "L1D: " << cache_config_.l1d_size_kb << " KB, "
              << "L2: " << cache_config_.l2_size_kb << " KB, "
              << "L3: " << cache_config_.l3_size_kb << " KB" << std::endl;
    std::cout << "  Execution Model: " << exec_model_name << std::endl;
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

    // Use execution model if available
    if (execution_model_) {
        // Event-driven/analytical model: advance in larger chunks efficiently
        const Cycle chunk_size = 1000;

        while (current_cycle_ < target_cycle) {
            // Check for messages from device
            handleDeviceMessages();

            // Advance execution model
            Cycle cycles_to_advance = std::min(chunk_size, target_cycle - current_cycle_);
            execution_model_->advanceCycles(cycles_to_advance);
            current_cycle_ = execution_model_->getCurrentCycle();

            // Synchronize with device periodically
            if (current_cycle_ % 1000 == 0) {
                synchronizeWithDevice();
            }

            stats_.total_cycles = current_cycle_;
        }
    } else {
        // Fallback: simple cycle-by-cycle advance
        while (current_cycle_ < target_cycle) {
            handleDeviceMessages();
            advanceCycle();

            if (current_cycle_ % 1000 == 0) {
                synchronizeWithDevice();
            }

            stats_.total_cycles = current_cycle_;
        }
    }

    std::cout << "Host Engine completed " << num_cycles << " cycles" << std::endl;
}

void HostEngine::finalize() {
    std::cout << "Finalizing Host Engine..." << std::endl;

    // Finalize execution model
    if (execution_model_) {
        // Get final stats from execution model
        auto exec_stats = execution_model_->getStats();
        stats_.total_cycles = std::max(stats_.total_cycles, exec_stats.total_cycles);
        stats_.total_instructions = exec_stats.total_instructions;

        execution_model_->finalize();
        execution_model_.reset();
    }

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

    // Store binary path for later use
    binary_path_ = binary_path;

    // If using ZSim execution model, launch simulation
    if (execution_model_ &&
        execution_model_->getType() == ExecutionModelType::ZSIM_EXECUTION_DRIVEN) {

        auto* zsim_model = dynamic_cast<ZSimExecutionModel*>(execution_model_.get());
        if (zsim_model) {
            std::cout << "Launching ZSim simulation for binary: " << binary_path << std::endl;

            // Launch ZSim with the binary
            if (!zsim_model->launchSimulation(binary_path, binary_args_)) {
                std::cerr << "Warning: Failed to launch ZSim simulation" << std::endl;
                std::cerr << "  Ensure PIN_HOME is set and ZSim is built" << std::endl;
                std::cerr << "  Falling back to analytical model" << std::endl;
            }
        }
    }
    // For analytical model, binary is used for workload characterization
    // (e.g., extracting task parameters from profiling data)
}

void HostEngine::setArguments(int argc, char** argv) {
    std::cout << "Setting program arguments (argc=" << argc << ")" << std::endl;

    // Store arguments for later use with ZSim
    binary_args_.clear();
    for (int i = 0; i < argc; i++) {
        if (argv[i]) {
            binary_args_.push_back(argv[i]);
        }
    }
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
    std::cout << "Initializing execution model for host simulation..." << std::endl;

    // Create execution model using factory
    execution_model_ = ExecutionModelFactory::createExecutionModel(
        execution_model_type_, config_, SimulationDomain::HOST);

    if (!execution_model_) {
        throw std::runtime_error("Failed to create execution model");
    }

    // Get config file path for execution model
    auto& cfg = pimid::config::ConfigManager::getInstance();
    std::string config_file = cfg.getString("host.execution_config", "");

    // Initialize the execution model
    if (!execution_model_->initialize(config_file, SimulationDomain::HOST)) {
        throw std::runtime_error("Failed to initialize execution model");
    }

    // Register memory model with execution model
    if (memory_model_) {
        execution_model_->registerMemoryModel(memory_model_);
    }

    // Register task completion callback
    execution_model_->registerTaskCompleteCallback(
        [this](const Task& task, Cycle completion_cycle) {
            stats_.total_tasks++;
            current_cycle_ = std::max(current_cycle_, completion_cycle);
        });

    std::cout << "Execution model initialized: " << execution_model_->getName() << std::endl;

    // Legacy pointer for backwards compatibility
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
                // Handle cross-domain memory request from device
                {
                    std::cout << "Received memory request from device: addr=0x"
                              << std::hex << msg.addr << std::dec
                              << " size=" << msg.size << std::endl;

                    // Process the memory request
                    MemoryRequest req;
                    req.addr = msg.addr;
                    req.size = msg.size;
                    req.type = (msg.data.size() > 0) ? MemoryRequestType::WRITE : MemoryRequestType::READ;

                    Cycle latency = 0;
                    if (memory_model_) {
                        latency = memory_model_->access(req);
                    } else {
                        latency = 100;  // Default latency when no memory model
                    }

                    // Send response back to device
                    CommMessage response;
                    response.type = MessageType::MEMORY_RESPONSE;
                    response.timestamp = latency;
                    response.src_domain = SimulationDomain::HOST;
                    response.dst_domain = SimulationDomain::DEVICE;
                    response.addr = msg.addr;
                    response.size = msg.size;
                    response.request_id = msg.request_id;

                    if (!device_comm_->sendMessage(response)) {
                        std::cerr << "Failed to send memory response to device" << std::endl;
                    }

                    stats_.memory_accesses++;
                }
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
