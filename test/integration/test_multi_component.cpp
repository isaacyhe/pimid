/**
 * @file test_multi_component.cpp
 * @brief Integration test for multi-component interactions
 * Tests the interaction between schedulers, address translator, and event queue
 */

#include "scheduler/scheduler.h"
#include "scheduler/nearest_scheduler.h"
#include "scheduler/roundrobin_scheduler.h"
#include "scheduler/loadbalanced_scheduler.h"
#include "address_translation/address_translator.h"
#include "address_translation/pe_placement.h"
#include "common/event_queue.h"
#include "common/types.h"
#include <iostream>
#include <cassert>
#include <memory>
#include <vector>

using namespace pimid;

// Test configuration
constexpr uint32_t NUM_PES = 8;
constexpr uint32_t NUM_TASKS = 100;
constexpr uint64_t MEMORY_SIZE = 1ULL << 30; // 1GB

/**
 * Test 1: Scheduler + Address Translator Integration
 * Verifies that scheduled tasks can be properly translated to PE addresses
 */
void test_scheduler_address_translator() {
    std::cout << "\n=== Test 1: Scheduler + Address Translator Integration ===" << std::endl;

    // Setup memory hierarchy
    MemoryHierarchy hierarchy;
    hierarchy.num_subarrays_per_bank = 16;
    hierarchy.num_banks_per_chip = 8;
    hierarchy.num_chips_per_rank = 8;
    hierarchy.num_ranks = 2;
    hierarchy.subarray_size_bytes = 8 * 1024 * 1024;  // 8MB
    hierarchy.bank_size_bytes = hierarchy.subarray_size_bytes * hierarchy.num_subarrays_per_bank;

    // Create PE placement manager
    PEPlacementManager pe_manager(hierarchy, PEPlacementLevel::BANK, AddressingMode::DISCRETE);

    // Register PEs
    for (uint32_t i = 0; i < NUM_PES; i++) {
        PEDescriptor pe;
        pe.pe_id = i;
        pe.level = PEPlacementLevel::BANK;
        pe.bank_id = i % 8;
        pe.chip_id = 0;
        pe.rank_id = 0;
        pe.addr_base = i * (MEMORY_SIZE / NUM_PES);
        pe.addr_limit = (i + 1) * (MEMORY_SIZE / NUM_PES);
        pe_manager.registerPE(pe);
    }

    // Create scheduler (test with nearest scheduler)
    NearestScheduler scheduler(pe_manager);
    scheduler.initialize();

    // Create address translator
    AddressTranslator translator(hierarchy, PEPlacementLevel::BANK, AddressingMode::DISCRETE);
    translator.initialize();

    // Schedule tasks and verify address translation
    uint32_t successful_schedules = 0;
    for (uint32_t task_id = 0; task_id < NUM_TASKS; task_id++) {
        Address data_addr = (task_id * (MEMORY_SIZE / NUM_TASKS)) % MEMORY_SIZE;

        // Schedule task
        uint32_t assigned_pe = scheduler.schedule(data_addr, SchedulingHint::PREFER_LOCAL);

        // Verify PE can access the address
        if (pe_manager.canAccess(assigned_pe, data_addr)) {
            successful_schedules++;
        }

        // Translate address for the PE
        auto translation = translator.translate(data_addr, assigned_pe);
        assert(translation.success && "Address translation should succeed");
    }

    std::cout << "  Scheduled " << successful_schedules << "/" << NUM_TASKS << " tasks successfully" << std::endl;
    assert(successful_schedules == NUM_TASKS && "All tasks should be scheduled successfully");

    std::cout << "  Test PASSED" << std::endl;
}

/**
 * Test 2: Event Queue + Scheduler Integration
 * Tests task scheduling with event-driven completion notifications
 */
void test_event_queue_scheduler() {
    std::cout << "\n=== Test 2: Event Queue + Scheduler Integration ===" << std::endl;

    // Create event queue
    EventQueue event_queue;

    // Setup memory hierarchy
    MemoryHierarchy hierarchy;
    hierarchy.num_subarrays_per_bank = 16;
    hierarchy.num_banks_per_chip = 8;
    hierarchy.num_chips_per_rank = 8;
    hierarchy.num_ranks = 2;
    hierarchy.subarray_size_bytes = 8 * 1024 * 1024;

    // Create PE placement manager
    PEPlacementManager pe_manager(hierarchy, PEPlacementLevel::BANK, AddressingMode::DISCRETE);

    // Register PEs
    for (uint32_t i = 0; i < NUM_PES; i++) {
        PEDescriptor pe;
        pe.pe_id = i;
        pe.level = PEPlacementLevel::BANK;
        pe.bank_id = i % 8;
        pe.addr_base = i * (MEMORY_SIZE / NUM_PES);
        pe.addr_limit = (i + 1) * (MEMORY_SIZE / NUM_PES);
        pe_manager.registerPE(pe);
    }

    // Create round-robin scheduler
    RoundRobinScheduler scheduler(pe_manager);
    scheduler.initialize();

    // Schedule tasks and add completion events
    Cycle current_cycle = 0;
    std::vector<uint32_t> scheduled_pes;

    for (uint32_t task_id = 0; task_id < NUM_TASKS; task_id++) {
        Address data_addr = (task_id * 1024) % MEMORY_SIZE;

        // Schedule task
        uint32_t assigned_pe = scheduler.schedule(data_addr, SchedulingHint::PREFER_LOCAL);
        scheduled_pes.push_back(assigned_pe);

        // Create completion event
        Event completion_event;
        completion_event.cycle = current_cycle + 100 + (task_id % 50); // Variable latency
        completion_event.type = EventType::TASK_COMPLETION;
        completion_event.task_id = task_id;
        completion_event.pe_id = assigned_pe;

        event_queue.insert(completion_event);
        current_cycle++;
    }

    // Process events
    uint32_t completed_tasks = 0;
    current_cycle = 0;

    while (!event_queue.empty()) {
        Event next_event = event_queue.peek();
        if (next_event.cycle > current_cycle + 10000) break; // Timeout

        event_queue.pop();
        completed_tasks++;

        // Mark PE as available for scheduling
        scheduler.onTaskComplete(next_event.pe_id, next_event.task_id);

        current_cycle = next_event.cycle;
    }

    std::cout << "  Completed " << completed_tasks << "/" << NUM_TASKS << " tasks" << std::endl;
    assert(completed_tasks == NUM_TASKS && "All tasks should complete");

    std::cout << "  Test PASSED" << std::endl;
}

/**
 * Test 3: Full Multi-Component Integration
 * Tests scheduler + address translator + event queue + PE statistics
 */
void test_full_integration() {
    std::cout << "\n=== Test 3: Full Multi-Component Integration ===" << std::endl;

    // Setup all components
    EventQueue event_queue;

    MemoryHierarchy hierarchy;
    hierarchy.num_subarrays_per_bank = 16;
    hierarchy.num_banks_per_chip = 8;
    hierarchy.num_chips_per_rank = 8;
    hierarchy.num_ranks = 2;
    hierarchy.subarray_size_bytes = 8 * 1024 * 1024;

    PEPlacementManager pe_manager(hierarchy, PEPlacementLevel::BANK, AddressingMode::DISCRETE);

    // Register PEs with detailed statistics tracking
    for (uint32_t i = 0; i < NUM_PES; i++) {
        PEDescriptor pe;
        pe.pe_id = i;
        pe.level = PEPlacementLevel::BANK;
        pe.bank_id = i % 8;
        pe.chip_id = 0;
        pe.rank_id = 0;
        pe.addr_base = i * (MEMORY_SIZE / NUM_PES);
        pe.addr_limit = (i + 1) * (MEMORY_SIZE / NUM_PES);
        pe.num_cores = 1;
        pe.frequency_mhz = 1000;
        pe_manager.registerPE(pe);
    }

    // Create load-balanced scheduler
    LoadBalancedScheduler scheduler(pe_manager);
    scheduler.initialize();

    AddressTranslator translator(hierarchy, PEPlacementLevel::BANK, AddressingMode::DISCRETE);
    translator.initialize();

    // Simulate workload with mixed access patterns
    Cycle current_cycle = 0;
    uint32_t local_accesses = 0;
    uint32_t remote_accesses = 0;
    uint32_t completed = 0;

    for (uint32_t task_id = 0; task_id < NUM_TASKS; task_id++) {
        // Mix of local and remote accesses
        Address data_addr = (task_id % 3 == 0)
            ? (task_id * 1024) % MEMORY_SIZE  // Sequential
            : (task_id * 1024 * 1024) % MEMORY_SIZE;  // Strided

        // Schedule with load balancing
        uint32_t assigned_pe = scheduler.schedule(data_addr, SchedulingHint::LOAD_BALANCE);

        // Check if access is local or remote
        if (pe_manager.isLocalAddress(assigned_pe, data_addr)) {
            local_accesses++;
        } else {
            remote_accesses++;
        }

        // Translate address
        auto translation = translator.translate(data_addr, assigned_pe);
        assert(translation.success);

        // Calculate latency based on locality
        Cycle latency = pe_manager.isLocalAddress(assigned_pe, data_addr) ? 100 : 200;

        // Schedule completion event
        Event completion;
        completion.cycle = current_cycle + latency;
        completion.type = EventType::TASK_COMPLETION;
        completion.task_id = task_id;
        completion.pe_id = assigned_pe;
        event_queue.insert(completion);

        current_cycle += 10;
    }

    // Process all events
    while (!event_queue.empty() && completed < NUM_TASKS) {
        Event next = event_queue.peek();
        event_queue.pop();

        scheduler.onTaskComplete(next.pe_id, next.task_id);
        completed++;
    }

    // Print statistics
    std::cout << "  Total tasks: " << NUM_TASKS << std::endl;
    std::cout << "  Completed: " << completed << std::endl;
    std::cout << "  Local accesses: " << local_accesses << std::endl;
    std::cout << "  Remote accesses: " << remote_accesses << std::endl;
    std::cout << "  Locality: " << (100.0 * local_accesses / NUM_TASKS) << "%" << std::endl;

    // Verify all tasks completed
    assert(completed == NUM_TASKS && "All tasks should complete");

    std::cout << "  Test PASSED" << std::endl;
}

int main() {
    std::cout << "PIMID Multi-Component Integration Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        test_scheduler_address_translator();
        test_event_queue_scheduler();
        test_full_integration();

        std::cout << "\n========================================" << std::endl;
        std::cout << "All integration tests PASSED!" << std::endl;
        std::cout << "========================================\n" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test FAILED with exception: " << e.what() << std::endl;
        return 1;
    }
}
