#include "execution_model/event_driven_execution_model.h"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace pimid {

EventDrivenExecutionModel::EventDrivenExecutionModel()
    : domain_(SimulationDomain::HOST)
    , num_cores_(1)
    , perf_model_(PerformanceModel::ROOFLINE)
    , current_cycle_(0)
    , initialized_(false)
{
    event_queue_ = std::make_shared<EventQueue>();
}

EventDrivenExecutionModel::~EventDrivenExecutionModel() {
    if (initialized_) {
        finalize();
    }
}

bool EventDrivenExecutionModel::initialize(const std::string& config_file,
                                           SimulationDomain domain) {
    if (initialized_) {
        std::cerr << "EventDrivenExecutionModel already initialized" << std::endl;
        return false;
    }

    config_file_ = config_file;
    domain_ = domain;

    try {
        std::cout << "Initializing Event-Driven Analytical Model for "
                  << (domain == SimulationDomain::HOST ? "HOST" : "DEVICE")
                  << " domain..." << std::endl;

        // Load configuration
        loadConfiguration(config_file);

        // Initialize core busy states
        core_busy_.resize(num_cores_, false);

        // Setup default core model
        core_model_.name = (domain == SimulationDomain::HOST) ? "Host CPU" : "PIM PE";
        core_model_.frequency_mhz = (domain == SimulationDomain::HOST) ? 2400.0 : 1000.0;
        core_model_.ipc = (domain == SimulationDomain::HOST) ? 2.0 : 1.0;
        core_model_.vector_width = (domain == SimulationDomain::HOST) ? 8 : 4;  // AVX-256 vs smaller SIMD
        core_model_.pipeline_depth = (domain == SimulationDomain::HOST) ? 14 : 5;

        initialized_ = true;
        std::cout << "Event-Driven model initialized with " << num_cores_
                  << " cores (IPC=" << core_model_.ipc << ", freq="
                  << core_model_.frequency_mhz << " MHz)" << std::endl;

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize Event-Driven model: " << e.what() << std::endl;
        return false;
    }
}

void EventDrivenExecutionModel::finalize() {
    if (!initialized_) {
        return;
    }

    std::cout << "Finalizing Event-Driven execution model..." << std::endl;

    // Process any remaining events
    if (event_queue_ && event_queue_->hasEvents()) {
        std::cout << "Warning: " << event_queue_->getNumEvents()
                  << " events remaining in queue" << std::endl;
    }

    // Clear queues
    while (!pending_tasks_.empty()) {
        pending_tasks_.pop();
    }
    active_tasks_.clear();

    initialized_ = false;
}

void EventDrivenExecutionModel::advanceCycles(Cycle num_cycles) {
    if (!initialized_) {
        throw std::runtime_error("Event-Driven model not initialized");
    }

    Cycle target_cycle = current_cycle_ + num_cycles;

    // Process all events up to target cycle
    // This is the key advantage of event-driven: we skip idle cycles!
    while (event_queue_->hasEvents() &&
           event_queue_->getNextEventCycle() <= target_cycle) {

        // Jump to next event (skip idle cycles!)
        current_cycle_ = event_queue_->getNextEventCycle();

        // Process all events at this cycle
        event_queue_->processEvents(current_cycle_);
    }

    // Advance to target
    current_cycle_ = target_cycle;
    stats_.total_cycles = current_cycle_;
}

Cycle EventDrivenExecutionModel::executeTask(const Task& task) {
    if (!initialized_) {
        throw std::runtime_error("Event-Driven model not initialized");
    }

    std::cout << "Executing task '" << task.kernel_name
              << "' (ID=" << task.task_id << ") on core " << task.pe_id << std::endl;

    // Estimate task execution cycles using analytical model
    Cycle execution_cycles = estimateTaskLatency(task);

    // Generate memory access pattern
    std::vector<MemoryAccess> accesses = generateMemoryAccesses(task);

    // Schedule memory accesses as events
    for (const auto& access : accesses) {
        Cycle access_cycle = current_cycle_ + access.issue_cycle;

        event_queue_->scheduleEvent(
            EventType::MEMORY_RESPONSE,
            access_cycle,
            0,  // Priority
            [this, access]() {
                this->processMemoryAccess(access);
            }
        );

        // Forward to memory model if available
        if (memory_model_) {
            MemoryRequest req{
                access.address,
                access.size,
                access.is_read ? MemoryRequest::Type::READ : MemoryRequest::Type::WRITE,
                access_cycle,
                access.core_id
            };
            // In async mode: memory_model_->access(req);
        }
    }

    // Calculate completion cycle
    Cycle completion_cycle = current_cycle_ + execution_cycles;

    // Schedule task completion event
    scheduleTaskCompletion(task, completion_cycle);

    // Mark core as busy
    if (task.pe_id < core_busy_.size()) {
        core_busy_[task.pe_id] = true;
    }

    // Add to active tasks
    active_tasks_[task.task_id] = task;

    // Update statistics
    stats_.total_tasks++;
    stats_.memory_accesses += accesses.size();

    std::cout << "  Estimated " << execution_cycles << " cycles, "
              << accesses.size() << " memory accesses, "
              << "completion at cycle " << completion_cycle << std::endl;

    return completion_cycle;
}

bool EventDrivenExecutionModel::isIdle() const {
    if (!initialized_) {
        return true;
    }

    // Check if any core is busy
    for (bool busy : core_busy_) {
        if (busy) {
            return false;
        }
    }

    // Check if any active tasks
    return active_tasks_.empty() && pending_tasks_.empty();
}

void EventDrivenExecutionModel::registerMemoryModel(std::shared_ptr<MemoryModel> memory_model) {
    memory_model_ = memory_model;
    std::cout << "Registered memory model with Event-Driven execution model" << std::endl;
}

std::vector<MemoryAccess> EventDrivenExecutionModel::getMemoryAccessPattern(const Task& task) const {
    return generateMemoryAccesses(task);
}

ExecutionStats EventDrivenExecutionModel::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    ExecutionStats stats = stats_;
    stats.total_cycles = current_cycle_;

    // Calculate metrics
    if (stats.total_cycles > 0) {
        stats.ipc = static_cast<double>(stats.total_instructions) / stats.total_cycles;

        // Utilization: fraction of core-cycles actually doing work
        uint64_t total_core_cycles = stats.total_cycles * num_cores_;
        stats.utilization = static_cast<double>(stats.total_instructions) /
                           (total_core_cycles * core_model_.ipc);
    }

    return stats;
}

void EventDrivenExecutionModel::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = ExecutionStats{};
}

Cycle EventDrivenExecutionModel::getCurrentCycle() const {
    return current_cycle_;
}

void EventDrivenExecutionModel::registerTaskCompleteCallback(
    std::function<void(const Task&, Cycle)> callback) {
    task_complete_callback_ = callback;
}

void EventDrivenExecutionModel::registerMemoryCallback(
    std::function<void(const MemoryAccess&, Cycle)> callback) {
    memory_callback_ = callback;
}

// -------------------------------------------------------------------------
// Event-Driven Specific Methods
// -------------------------------------------------------------------------

void EventDrivenExecutionModel::registerPEPlugin(std::shared_ptr<plugin::IPEPlugin> pe_plugin) {
    pe_plugin_ = pe_plugin;
    std::cout << "Registered PE plugin for analytical modeling" << std::endl;
}

void EventDrivenExecutionModel::setNumCores(uint32_t num_cores) {
    num_cores_ = num_cores;
    core_busy_.resize(num_cores, false);
}

void EventDrivenExecutionModel::setPerformanceModel(PerformanceModel model) {
    perf_model_ = model;
    std::cout << "Set performance model to: ";
    switch (model) {
        case PerformanceModel::ROOFLINE:
            std::cout << "Roofline" << std::endl;
            break;
        case PerformanceModel::CONFIGURABLE_IPC:
            std::cout << "Configurable IPC" << std::endl;
            break;
        case PerformanceModel::PLUGIN_BASED:
            std::cout << "Plugin-based" << std::endl;
            break;
    }
}

// -------------------------------------------------------------------------
// Private Methods
// -------------------------------------------------------------------------

void EventDrivenExecutionModel::loadConfiguration(const std::string& config_file) {
    // TODO: Load from actual YAML config file
    // For now, use defaults

    std::cout << "Loading configuration from: " << config_file << std::endl;

    // Default: 4 host cores or 256 PIM PEs
    num_cores_ = (domain_ == SimulationDomain::HOST) ? 4 : 256;
}

Cycle EventDrivenExecutionModel::estimateTaskLatency(const Task& task) const {
    switch (perf_model_) {
        case PerformanceModel::ROOFLINE:
            return rooflineModel(task);
        case PerformanceModel::CONFIGURABLE_IPC:
            return configurableIPCModel(task);
        case PerformanceModel::PLUGIN_BASED:
            return pluginBasedModel(task);
        default:
            return task.num_ops;  // Fallback: 1 cycle per op
    }
}

std::vector<MemoryAccess> EventDrivenExecutionModel::generateMemoryAccesses(const Task& task) const {
    std::vector<MemoryAccess> accesses;

    // Generate memory accesses based on task parameters
    // Use cache line granularity for efficiency

    const uint64_t cache_line_size = 64;

    // Input accesses (reads)
    for (size_t i = 0; i < task.input_addresses.size(); i++) {
        uint64_t base_addr = task.input_addresses[i];
        uint64_t size = task.input_size / task.input_addresses.size();
        auto input_accesses = generateSequentialAccesses(
            base_addr, size, true, 0);
        accesses.insert(accesses.end(), input_accesses.begin(), input_accesses.end());
    }

    // Output accesses (writes)
    for (size_t i = 0; i < task.output_addresses.size(); i++) {
        uint64_t base_addr = task.output_addresses[i];
        uint64_t size = task.output_size / task.output_addresses.size();
        auto output_accesses = generateSequentialAccesses(
            base_addr, size, false, 0);
        accesses.insert(accesses.end(), output_accesses.begin(), output_accesses.end());
    }

    // Set core ID for all accesses
    for (auto& access : accesses) {
        access.core_id = task.pe_id;
    }

    return accesses;
}

void EventDrivenExecutionModel::scheduleTaskCompletion(const Task& task, Cycle completion_cycle) {
    event_queue_->scheduleEvent(
        EventType::CUSTOM,  // Task completion
        completion_cycle,
        1,  // Higher priority than memory events
        [this, task]() {
            this->processTaskCompletion(task);
        }
    );
}

void EventDrivenExecutionModel::processTaskCompletion(const Task& task) {
    std::cout << "Task " << task.task_id << " (" << task.kernel_name
              << ") completed at cycle " << current_cycle_ << std::endl;

    // Mark core as idle
    if (task.pe_id < core_busy_.size()) {
        core_busy_[task.pe_id] = false;
    }

    // Remove from active tasks
    active_tasks_.erase(task.task_id);

    // Update statistics
    stats_.total_instructions += task.num_ops;

    // Trigger callback
    if (task_complete_callback_) {
        task_complete_callback_(task, current_cycle_);
    }

    // Schedule next pending task if available
    if (!pending_tasks_.empty()) {
        Task next_task = pending_tasks_.front();
        pending_tasks_.pop();
        executeTask(next_task);
    }
}

void EventDrivenExecutionModel::processMemoryAccess(const MemoryAccess& access) {
    if (memory_callback_) {
        memory_callback_(access, current_cycle_);
    }
}

// -------------------------------------------------------------------------
// Analytical Models
// -------------------------------------------------------------------------

Cycle EventDrivenExecutionModel::rooflineModel(const Task& task) const {
    // Roofline model: performance bounded by either compute or memory

    // Compute time (assuming ideal pipeline utilization)
    double ops_per_cycle = core_model_.ipc * core_model_.vector_width;
    Cycle compute_cycles = std::ceil(task.num_ops / ops_per_cycle);

    // Memory time (assuming memory bandwidth limit)
    // Typical DRAM bandwidth: ~25 GB/s for DDR4, ~300 GB/s for HBM2
    double memory_bandwidth_gbps = (domain_ == SimulationDomain::HOST) ? 25.0 : 300.0;
    double bytes_per_cycle = (memory_bandwidth_gbps * 1000.0) / core_model_.frequency_mhz;

    uint64_t total_bytes = task.input_size + task.output_size;
    Cycle memory_cycles = std::ceil(total_bytes / bytes_per_cycle);

    // Roofline: bounded by max of compute or memory
    Cycle total_cycles = std::max(compute_cycles, memory_cycles);

    return total_cycles;
}

Cycle EventDrivenExecutionModel::configurableIPCModel(const Task& task) const {
    // Simple IPC-based model
    Cycle cycles = std::ceil(task.num_ops / core_model_.ipc);

    // Add pipeline fill/drain overhead
    cycles += core_model_.pipeline_depth;

    return cycles;
}

Cycle EventDrivenExecutionModel::pluginBasedModel(const Task& task) const {
    if (!pe_plugin_) {
        // Fallback to configurable IPC model
        return configurableIPCModel(task);
    }

    // Use PE plugin to estimate latency
    // PE plugin should provide operation-specific latencies
    // For now, use simple approach
    return task.num_ops;  // TODO: Query plugin for operation latencies
}

// -------------------------------------------------------------------------
// Memory Access Pattern Generators
// -------------------------------------------------------------------------

std::vector<MemoryAccess> EventDrivenExecutionModel::generateSequentialAccesses(
    uint64_t base_addr, uint64_t size, bool is_read, Cycle start_cycle) const {

    std::vector<MemoryAccess> accesses;
    const uint64_t cache_line_size = 64;

    // Generate cache line-aligned accesses
    uint64_t num_lines = (size + cache_line_size - 1) / cache_line_size;

    for (uint64_t i = 0; i < num_lines; i++) {
        uint64_t addr = base_addr + i * cache_line_size;
        uint64_t access_size = std::min(cache_line_size, size - i * cache_line_size);

        accesses.push_back({
            addr,
            access_size,
            is_read,
            start_cycle + i,  // Spread accesses over time
            0,  // Core ID set by caller
            MemoryAccess::Pattern::SEQUENTIAL,
            0
        });
    }

    return accesses;
}

std::vector<MemoryAccess> EventDrivenExecutionModel::generateStridedAccesses(
    uint64_t base_addr, uint64_t size, uint64_t stride,
    bool is_read, Cycle start_cycle) const {

    std::vector<MemoryAccess> accesses;

    uint64_t num_accesses = size / stride;

    for (uint64_t i = 0; i < num_accesses; i++) {
        uint64_t addr = base_addr + i * stride;

        accesses.push_back({
            addr,
            stride,
            is_read,
            start_cycle + i,
            0,  // Core ID set by caller
            MemoryAccess::Pattern::STRIDED,
            stride
        });
    }

    return accesses;
}

// -------------------------------------------------------------------------
// Hybrid Execution Model
// -------------------------------------------------------------------------

HybridExecutionModel::HybridExecutionModel(
    std::shared_ptr<IExecutionModel> host_model,
    std::shared_ptr<IExecutionModel> device_model)
    : host_model_(host_model)
    , device_model_(device_model)
    , current_domain_(SimulationDomain::HOST)
{
}

HybridExecutionModel::~HybridExecutionModel() {
}

bool HybridExecutionModel::initialize(const std::string& config_file,
                                     SimulationDomain domain) {
    current_domain_ = domain;

    // Initialize both models
    bool host_ok = host_model_->initialize(config_file, SimulationDomain::HOST);
    bool device_ok = device_model_->initialize(config_file, SimulationDomain::DEVICE);

    return host_ok && device_ok;
}

void HybridExecutionModel::finalize() {
    host_model_->finalize();
    device_model_->finalize();
}

void HybridExecutionModel::advanceCycles(Cycle num_cycles) {
    // Advance both models synchronously
    host_model_->advanceCycles(num_cycles);
    device_model_->advanceCycles(num_cycles);
}

Cycle HybridExecutionModel::executeTask(const Task& task) {
    // Route to appropriate model based on current domain
    if (current_domain_ == SimulationDomain::HOST) {
        return host_model_->executeTask(task);
    } else {
        return device_model_->executeTask(task);
    }
}

bool HybridExecutionModel::isIdle() const {
    return host_model_->isIdle() && device_model_->isIdle();
}

void HybridExecutionModel::registerMemoryModel(std::shared_ptr<MemoryModel> memory_model) {
    host_model_->registerMemoryModel(memory_model);
    device_model_->registerMemoryModel(memory_model);
}

std::vector<MemoryAccess> HybridExecutionModel::getMemoryAccessPattern(const Task& task) const {
    if (current_domain_ == SimulationDomain::HOST) {
        return host_model_->getMemoryAccessPattern(task);
    } else {
        return device_model_->getMemoryAccessPattern(task);
    }
}

ExecutionStats HybridExecutionModel::getStats() const {
    // Combine stats from both models
    auto host_stats = host_model_->getStats();
    auto device_stats = device_model_->getStats();

    ExecutionStats combined;
    combined.total_instructions = host_stats.total_instructions + device_stats.total_instructions;
    combined.total_cycles = std::max(host_stats.total_cycles, device_stats.total_cycles);
    combined.total_tasks = host_stats.total_tasks + device_stats.total_tasks;
    combined.memory_accesses = host_stats.memory_accesses + device_stats.memory_accesses;

    if (combined.total_cycles > 0) {
        combined.ipc = static_cast<double>(combined.total_instructions) / combined.total_cycles;
    }

    return combined;
}

void HybridExecutionModel::resetStats() {
    host_model_->resetStats();
    device_model_->resetStats();
}

Cycle HybridExecutionModel::getCurrentCycle() const {
    // Return max cycle of both domains
    return std::max(host_model_->getCurrentCycle(),
                   device_model_->getCurrentCycle());
}

void HybridExecutionModel::registerTaskCompleteCallback(
    std::function<void(const Task&, Cycle)> callback) {
    host_model_->registerTaskCompleteCallback(callback);
    device_model_->registerTaskCompleteCallback(callback);
}

void HybridExecutionModel::registerMemoryCallback(
    std::function<void(const MemoryAccess&, Cycle)> callback) {
    host_model_->registerMemoryCallback(callback);
    device_model_->registerMemoryCallback(callback);
}

} // namespace pimid
