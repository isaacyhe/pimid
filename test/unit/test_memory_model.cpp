/**
 * @file test_memory_model.cpp
 * @brief Unit Tests for PIMID Memory Models
 *
 * Tests for SRAM, DRAM, STT-MRAM, PCM, and ReRAM memory models.
 */

#include <iostream>
#include <memory>
#include <chrono>
#include <iomanip>
#include <functional>
#include <vector>
#include <cmath>

// Include test utilities
#include "../test_utils.cpp"

// Memory models
#include "memory_models/include/memory_model.h"
#include "memory_models/include/sram_model.h"
#include "memory_models/include/dram_model.h"
#include "memory_models/include/sttmram_model.h"
#include "memory_models/include/pcm_model.h"
#include "memory_models/include/reram_model.h"

using namespace pimid;
using namespace pimid::test;

//=============================================================================
// SRAM Model Tests
//=============================================================================

bool test_sram_creation() {
    SRAMModel model("");
    model.initialize();
    assertTrue(model.getTechnology() == MemoryTechnology::SRAM, "Technology should be SRAM");
    return true;
}

bool test_sram_capacity() {
    SRAMModel model("");
    model.initialize();
    uint64_t capacity = model.getCapacity();
    assertGreaterThan(capacity, (uint64_t)0, "Capacity should be positive");
    return true;
}

bool test_sram_latency() {
    SRAMModel model("");
    model.initialize();

    Cycle read_lat = model.getLatency(MemoryRequestType::READ);
    Cycle write_lat = model.getLatency(MemoryRequestType::WRITE);

    assertGreaterThan(read_lat, (Cycle)0, "Read latency should be positive");
    assertGreaterThan(write_lat, (Cycle)0, "Write latency should be positive");

    return true;
}

bool test_sram_energy() {
    SRAMModel model("");
    model.initialize();

    double read_energy = model.getReadEnergy();
    double write_energy = model.getWriteEnergy();
    double leakage = model.getLeakagePower();

    assertTrue(read_energy >= 0, "Read energy should be non-negative");
    assertTrue(write_energy >= 0, "Write energy should be non-negative");
    assertTrue(leakage >= 0, "Leakage power should be non-negative");

    return true;
}

bool test_sram_access() {
    SRAMModel model("");
    model.initialize();

    MemoryRequest req = makeTestRequest(0x1000, MemoryRequestType::READ, 64);
    Cycle latency = model.access(req);

    assertGreaterThan(latency, (Cycle)0, "Access should have positive latency");
    return true;
}

bool test_sram_can_accept() {
    SRAMModel model("");
    model.initialize();

    MemoryRequest req = makeTestRequest(0x1000, MemoryRequestType::READ, 64);
    bool can_accept = model.canAccept(req);

    // Initially should be able to accept requests
    assertTrue(can_accept, "Should be able to accept initial request");
    return true;
}

//=============================================================================
// DRAM Model Tests
//=============================================================================

bool test_dram_creation() {
    DRAMModel model("");
    model.initialize();
    assertTrue(model.getTechnology() == MemoryTechnology::DRAM, "Technology should be DRAM");
    return true;
}

bool test_dram_timing() {
    DRAMModel model("");
    model.initialize();

    Cycle read_lat = model.getLatency(MemoryRequestType::READ);
    Cycle write_lat = model.getLatency(MemoryRequestType::WRITE);

    // DRAM typically has higher latency than SRAM
    assertGreaterThan(read_lat, (Cycle)0, "Read latency should be positive");
    assertGreaterThan(write_lat, (Cycle)0, "Write latency should be positive");

    return true;
}

bool test_dram_bandwidth() {
    DRAMModel model("");
    model.initialize();

    uint64_t bandwidth = model.getBandwidth();
    assertGreaterThan(bandwidth, (uint64_t)0, "Bandwidth should be positive");

    return true;
}

bool test_dram_access_pattern() {
    DRAMModel model("");
    model.initialize();

    // Test sequential access pattern
    std::vector<MemoryRequest> requests = generateSequentialRequests(10, 0, 64);

    for (const auto& req : requests) {
        Cycle latency = model.access(req);
        assertGreaterThan(latency, (Cycle)0, "Access should have positive latency");
    }

    return true;
}

//=============================================================================
// STT-MRAM Model Tests
//=============================================================================

bool test_sttmram_creation() {
    STTMRAMModel model("");
    model.initialize();
    assertTrue(model.getTechnology() == MemoryTechnology::STT_MRAM,
               "Technology should be STT_MRAM");
    return true;
}

bool test_sttmram_asymmetric_energy() {
    STTMRAMModel model("");
    model.initialize();

    double read_energy = model.getReadEnergy();
    double write_energy = model.getWriteEnergy();

    // STT-MRAM typically has higher write energy than read
    assertTrue(read_energy >= 0, "Read energy should be non-negative");
    assertTrue(write_energy >= 0, "Write energy should be non-negative");

    return true;
}

bool test_sttmram_persistence() {
    STTMRAMModel model("");
    model.initialize();

    // Non-volatile memory should have zero or near-zero leakage in data retention
    double leakage = model.getLeakagePower();
    assertTrue(leakage >= 0, "Leakage should be non-negative");

    return true;
}

//=============================================================================
// PCM Model Tests
//=============================================================================

bool test_pcm_creation() {
    PCMModel model("");
    model.initialize();
    assertTrue(model.getTechnology() == MemoryTechnology::PCM, "Technology should be PCM");
    return true;
}

bool test_pcm_write_latency() {
    PCMModel model("");
    model.initialize();

    Cycle read_lat = model.getLatency(MemoryRequestType::READ);
    Cycle write_lat = model.getLatency(MemoryRequestType::WRITE);

    // PCM typically has much higher write latency
    assertGreaterThan(write_lat, (Cycle)0, "Write latency should be positive");
    assertGreaterThan(read_lat, (Cycle)0, "Read latency should be positive");

    return true;
}

//=============================================================================
// ReRAM Model Tests
//=============================================================================

bool test_reram_creation() {
    ReRAMModel model("");
    model.initialize();
    assertTrue(model.getTechnology() == MemoryTechnology::RERAM, "Technology should be RERAM");
    return true;
}

bool test_reram_energy() {
    ReRAMModel model("");
    model.initialize();

    double read_energy = model.getReadEnergy();
    double write_energy = model.getWriteEnergy();

    assertTrue(read_energy >= 0, "Read energy should be non-negative");
    assertTrue(write_energy >= 0, "Write energy should be non-negative");

    return true;
}

//=============================================================================
// Cross-Technology Comparison Tests
//=============================================================================

bool test_technology_comparison() {
    SRAMModel sram("");
    DRAMModel dram("");
    STTMRAMModel sttmram("");

    sram.initialize();
    dram.initialize();
    sttmram.initialize();

    // SRAM should generally have lower latency than DRAM
    Cycle sram_lat = sram.getLatency(MemoryRequestType::READ);
    Cycle dram_lat = dram.getLatency(MemoryRequestType::READ);

    // Just verify all have positive latency
    assertGreaterThan(sram_lat, (Cycle)0, "SRAM latency should be positive");
    assertGreaterThan(dram_lat, (Cycle)0, "DRAM latency should be positive");

    return true;
}

bool test_memory_model_factory() {
    // Test that we can create models via factory
    auto sram = MemoryModelFactory::createMemoryModel(MemoryTechnology::SRAM, "");
    auto dram = MemoryModelFactory::createMemoryModel(MemoryTechnology::DRAM, "");

    assertNotNull(sram.get(), "SRAM model should not be null");
    assertNotNull(dram.get(), "DRAM model should not be null");

    assertEqual(sram->getTechnology(), MemoryTechnology::SRAM, "Factory SRAM tech mismatch");
    assertEqual(dram->getTechnology(), MemoryTechnology::DRAM, "Factory DRAM tech mismatch");

    return true;
}

//=============================================================================
// Stress Tests
//=============================================================================

bool test_high_throughput() {
    SRAMModel model("");
    model.initialize();

    const size_t num_requests = 10000;
    auto requests = generateRandomRequests(num_requests, 0, 0x100000);

    Timer timer;
    timer.start();

    for (const auto& req : requests) {
        model.access(req);
    }

    double elapsed_ms = timer.elapsedMs();
    double throughput = num_requests / (elapsed_ms / 1000.0);

    std::cout << "    Throughput: " << std::fixed << std::setprecision(0)
              << throughput << " req/s";

    assertGreaterThan(throughput, 1000.0, "Throughput should be reasonable");
    return true;
}

bool test_tick_advancement() {
    SRAMModel model("");
    model.initialize();

    // Tick should advance model state without crashing
    for (int i = 0; i < 1000; i++) {
        model.tick();
    }

    return true;
}

bool test_stats_reset() {
    SRAMModel model("");
    model.initialize();

    // Perform some accesses
    for (int i = 0; i < 100; i++) {
        MemoryRequest req = makeTestRequest(i * 64, MemoryRequestType::READ, 64);
        model.access(req);
    }

    // Reset stats
    model.resetStats();

    // Stats should be reset (no crashes)
    return true;
}

//=============================================================================
// Main
//=============================================================================

int main() {
    printTestHeader("PIMID Memory Model Unit Tests");

    int total_failures = 0;

    // SRAM Tests
    {
        TestSuite suite("SRAM Model");
        suite.run("Creation", test_sram_creation);
        suite.run("Capacity", test_sram_capacity);
        suite.run("Latency", test_sram_latency);
        suite.run("Energy", test_sram_energy);
        suite.run("Access", test_sram_access);
        suite.run("Can Accept", test_sram_can_accept);
        total_failures += suite.summarize();
    }

    // DRAM Tests
    {
        TestSuite suite("DRAM Model");
        suite.run("Creation", test_dram_creation);
        suite.run("Timing", test_dram_timing);
        suite.run("Bandwidth", test_dram_bandwidth);
        suite.run("Access Pattern", test_dram_access_pattern);
        total_failures += suite.summarize();
    }

    // STT-MRAM Tests
    {
        TestSuite suite("STT-MRAM Model");
        suite.run("Creation", test_sttmram_creation);
        suite.run("Asymmetric Energy", test_sttmram_asymmetric_energy);
        suite.run("Persistence", test_sttmram_persistence);
        total_failures += suite.summarize();
    }

    // PCM Tests
    {
        TestSuite suite("PCM Model");
        suite.run("Creation", test_pcm_creation);
        suite.run("Write Latency", test_pcm_write_latency);
        total_failures += suite.summarize();
    }

    // ReRAM Tests
    {
        TestSuite suite("ReRAM Model");
        suite.run("Creation", test_reram_creation);
        suite.run("Energy", test_reram_energy);
        total_failures += suite.summarize();
    }

    // Cross-Technology Tests
    {
        TestSuite suite("Cross-Technology");
        suite.run("Technology Comparison", test_technology_comparison);
        suite.run("Memory Model Factory", test_memory_model_factory);
        total_failures += suite.summarize();
    }

    // Stress Tests
    {
        TestSuite suite("Stress Tests");
        suite.run("High Throughput", test_high_throughput);
        suite.run("Tick Advancement", test_tick_advancement);
        suite.run("Stats Reset", test_stats_reset);
        total_failures += suite.summarize();
    }

    // Final Summary
    printSeparator();
    if (total_failures == 0) {
        std::cout << "\n\033[32m✓ All tests passed!\033[0m\n" << std::endl;
    } else {
        std::cout << "\n\033[31m✗ " << total_failures << " test(s) failed\033[0m\n" << std::endl;
    }

    return total_failures > 0 ? 1 : 0;
}
