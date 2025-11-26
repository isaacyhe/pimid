#include "execution_model/zsim_execution_model.h"
#include "external/zsim/src/zsim.h"
#include "external/zsim/src/init.h"
#include "external/zsim/src/core.h"
#include <iostream>
#include <stdexcept>

namespace pimid {

ZSimExecutionModel::ZSimExecutionModel()
    : zsim_glob_info_(nullptr)
    , cores_(nullptr)
    , num_cores_(0)
    , domain_(SimulationDomain::HOST)
    , pim_mode_(false)
    , current_cycle_(0)
    , initialized_(false)
    , idle_(true)
{
}

ZSimExecutionModel::~ZSimExecutionModel() {
    if (initialized_) {
        finalize();
    }
}

bool ZSimExecutionModel::initialize(const std::string& config_file,
                                    SimulationDomain domain) {
    if (initialized_) {
        std::cerr << "ZSimExecutionModel already initialized" << std::endl;
        return false;
    }

    config_file_ = config_file;
    domain_ = domain;

    try {
        // Initialize ZSim
        // Based on MultiPIM approach: use configuration file for setup
        std::cout << "Initializing ZSim for "
                  << (domain == SimulationDomain::HOST ? "HOST" : "DEVICE")
                  << " domain..." << std::endl;

        initializeZSim(config_file);

        // Setup memory interception for Ramulator integration
        // Based on ramulator-pim approach
        setupMemoryInterception();

        // Connect to PIMID's Ramulator instance
        connectToRamulator();

        initialized_ = true;
        std::cout << "ZSim execution model initialized successfully with "
                  << num_cores_ << " cores" << std::endl;

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize ZSim: " << e.what() << std::endl;
        return false;
    }
}

void ZSimExecutionModel::finalize() {
    if (!initialized_) {
        return;
    }

    std::cout << "Finalizing ZSim execution model..." << std::endl;

    // Clean up ZSim resources
    // Note: ZSim doesn't have a clean shutdown API, so we just null out pointers
    zsim_glob_info_ = nullptr;
    cores_ = nullptr;
    num_cores_ = 0;

    initialized_ = false;
}

void ZSimExecutionModel::advanceCycles(Cycle num_cycles) {
    if (!initialized_) {
        throw std::runtime_error("ZSim not initialized");
    }

    // ZSim uses its own phase-based execution
    // We need to advance the simulation to the target cycle
    Cycle target_cycle = current_cycle_ + num_cycles;

    // In a full implementation, we would:
    // 1. Run ZSim bound phase
    // 2. Process weave phase for contention
    // 3. Synchronize with memory/network models

    // For now, placeholder that tracks cycles
    current_cycle_ = target_cycle;
    stats_.total_cycles = current_cycle_;

    // Update idle state
    idle_ = isIdle();
}

Cycle ZSimExecutionModel::executeTask(const Task& task) {
    if (!initialized_) {
        throw std::runtime_error("ZSim not initialized");
    }

    // For execution-driven model, tasks are executed via actual code
    // This is different from event-driven where we analytically model tasks

    // In a full implementation, this would:
    // 1. Trigger PIM offload using zsim_hooks
    // 2. Execute the kernel code through PIN instrumentation
    // 3. Return when execution completes

    // For now, return estimated completion based on analytical model
    // (This will be replaced with actual ZSim execution)
    Cycle estimated_cycles = task.estimated_cycles;
    if (estimated_cycles == 0) {
        // Rough estimate: 1 cycle per operation
        estimated_cycles = task.num_ops;
    }

    Cycle completion_cycle = current_cycle_ + estimated_cycles;

    // Update statistics
    stats_.total_tasks++;
    stats_.total_instructions += task.num_ops;

    // Trigger callback
    if (task_complete_callback_) {
        task_complete_callback_(task, completion_cycle);
    }

    return completion_cycle;
}

bool ZSimExecutionModel::isIdle() const {
    if (!initialized_ || !zsim_glob_info_) {
        return true;
    }

    // Check if any core is executing
    // In full implementation, query ZSim core states
    return idle_.load();
}

void ZSimExecutionModel::registerMemoryModel(std::shared_ptr<MemoryModel> memory_model) {
    memory_model_ = memory_model;
    std::cout << "Registered memory model with ZSim execution model" << std::endl;
}

std::vector<MemoryAccess> ZSimExecutionModel::getMemoryAccessPattern(const Task& task) const {
    // ZSim generates memory accesses dynamically during execution
    // For analysis purposes, we can estimate the pattern

    std::vector<MemoryAccess> accesses;

    // Generate memory accesses for inputs
    for (size_t i = 0; i < task.input_addresses.size(); i++) {
        uint64_t addr = task.input_addresses[i];
        uint64_t size = task.input_size / task.input_addresses.size();

        accesses.push_back({
            addr,
            size,
            true,  // read
            current_cycle_,
            task.pe_id,
            MemoryAccess::Pattern::SEQUENTIAL,
            0
        });
    }

    // Generate memory accesses for outputs
    for (size_t i = 0; i < task.output_addresses.size(); i++) {
        uint64_t addr = task.output_addresses[i];
        uint64_t size = task.output_size / task.output_addresses.size();

        accesses.push_back({
            addr,
            size,
            false,  // write
            current_cycle_,
            task.pe_id,
            MemoryAccess::Pattern::SEQUENTIAL,
            0
        });
    }

    return accesses;
}

ExecutionStats ZSimExecutionModel::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    ExecutionStats stats = stats_;
    stats.total_cycles = current_cycle_;

    // Calculate IPC
    if (stats.total_cycles > 0) {
        stats.ipc = static_cast<double>(stats.total_instructions) / stats.total_cycles;
    }

    return stats;
}

void ZSimExecutionModel::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = ExecutionStats{};
}

Cycle ZSimExecutionModel::getCurrentCycle() const {
    return current_cycle_.load();
}

void ZSimExecutionModel::registerTaskCompleteCallback(
    std::function<void(const Task&, Cycle)> callback) {
    task_complete_callback_ = callback;
}

void ZSimExecutionModel::registerMemoryCallback(
    std::function<void(const MemoryAccess&, Cycle)> callback) {
    memory_callback_ = callback;
}

Core* ZSimExecutionModel::getCore(uint32_t core_id) const {
    if (!initialized_ || !cores_ || core_id >= num_cores_) {
        return nullptr;
    }
    return cores_[core_id];
}

void ZSimExecutionModel::injectMemoryResponse(uint64_t address, Cycle latency) {
    // Interface for Ramulator to provide memory response back to ZSim
    // Based on ramulator-pim integration

    if (memory_callback_) {
        MemoryAccess access{
            address,
            64,  // Cache line size
            true,
            current_cycle_,
            0,
            MemoryAccess::Pattern::SEQUENTIAL,
            0
        };
        memory_callback_(access, latency);
    }
}

void ZSimExecutionModel::configurePIMMode(bool enable_pim, uint32_t num_pim_cores) {
    pim_mode_ = enable_pim;
    if (enable_pim) {
        std::cout << "Configured ZSim for PIM mode with "
                  << num_pim_cores << " PIM cores" << std::endl;
    }
}

// -------------------------------------------------------------------------
// Private Methods
// -------------------------------------------------------------------------

void ZSimExecutionModel::initializeZSim(const std::string& config_file) {
    // Initialize ZSim with configuration file
    // Based on MultiPIM's initialization approach

    // In a full implementation, we would:
    // 1. Call SimInit() with zsim config file
    // 2. Get GlobSimInfo pointer
    // 3. Extract cores array
    // 4. Setup instrumentation hooks

    // For now, create placeholder
    // TODO: Actual ZSim initialization
    std::cout << "Loading ZSim configuration from: " << config_file << std::endl;
    std::cout << "Note: Full ZSim integration pending - using placeholder" << std::endl;

    // Placeholder: Set to nullptr to indicate not fully integrated
    zsim_glob_info_ = nullptr;
    cores_ = nullptr;
    num_cores_ = (domain_ == SimulationDomain::HOST) ? 4 : 256;  // Default values
}

void ZSimExecutionModel::setupMemoryInterception() {
    // Setup memory request interception to feed into PIMID's Ramulator
    // Based on ramulator-pim's approach

    // In full implementation:
    // 1. Register callbacks with ZSim's memory controller
    // 2. Intercept memory requests before they hit ZSim's DDR model
    // 3. Forward to PIMID's Ramulator instance
    // 4. Inject response back when Ramulator completes

    std::cout << "Setting up memory interception for Ramulator integration" << std::endl;
}

void ZSimExecutionModel::connectToRamulator() {
    // Connect ZSim memory requests to PIMID's Ramulator instance
    // This is the key integration point!

    if (memory_model_) {
        std::cout << "Connected ZSim to PIMID Ramulator memory model" << std::endl;
    } else {
        std::cout << "Warning: No memory model registered" << std::endl;
    }
}

// Static callbacks called by ZSim
void ZSimExecutionModel::zsimMemoryRequestCallback(void* ctx, uint64_t address,
                                                   uint64_t size, bool is_write) {
    auto* model = static_cast<ZSimExecutionModel*>(ctx);

    // Forward memory request to PIMID's memory model
    if (model->memory_model_) {
        MemoryRequest req{
            address,
            size,
            is_write ? MemoryRequest::Type::WRITE : MemoryRequest::Type::READ,
            model->current_cycle_,
            0  // Core ID
        };

        // In full implementation, this would be async
        // Cycle latency = model->memory_model_->access(req);
        // model->injectMemoryResponse(address, latency);
    }

    // Update statistics
    model->stats_.memory_accesses++;
}

void ZSimExecutionModel::zsimCycleCallback(void* ctx, uint64_t cycle) {
    auto* model = static_cast<ZSimExecutionModel*>(ctx);
    model->current_cycle_ = cycle;
}

} // namespace pimid
