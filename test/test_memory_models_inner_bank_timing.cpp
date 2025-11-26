/**
 * @file test_memory_models_inner_bank_timing.cpp
 * @brief Comprehensive test suite for all memory models with inner-bank timing
 *
 * Tests:
 * - SRAM model with CACTI integration
 * - STT-MRAM model with NVSim integration
 * - PCM model with very slow writes
 * - ReRAM model with analog compute support
 *
 * Validates:
 * - Inner-bank timing calculations
 * - Subarray, bank, and chip latencies
 * - PIM granularity support
 * - Energy modeling
 */

#include <iostream>
#include <cassert>
#include <memory>
#include <iomanip>

#include "memory_models/include/memory_model.h"
#include "memory_models/include/sram_model.h"
#include "memory_models/include/sttmram_model.h"
#include "memory_models/include/pcm_model.h"
#include "memory_models/include/reram_model.h"

using namespace pimid;

//=============================================================================
// Test Utilities
//=============================================================================

void printTestHeader(const std::string& test_name) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "TEST: " << test_name << std::endl;
    std::cout << "========================================" << std::endl;
}

void printTestResult(const std::string& test_name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << test_name << std::endl;
}

//=============================================================================
// SRAM Model Tests
//=============================================================================

void testSRAMModel() {
    printTestHeader("SRAM Model with Inner-Bank Timing");

    // Create SRAM model
    auto sram = std::make_shared<SRAMModel>("dummy_config.yaml");
    sram->initialize();

    // Test 1: Check inner-bank timing values
    std::cout << "\n--- Test 1: Inner-Bank Timing Values ---" << std::endl;
    double subarray_read_lat = sram->getSubarrayReadLatency();
    double bank_read_lat = sram->getBankReadLatency();
    double chip_read_lat = sram->getChipReadLatency();
    double inner_bank_datapath = sram->getInnerBankDatapathLatency();

    std::cout << "Subarray read latency: " << subarray_read_lat << " ns" << std::endl;
    std::cout << "Bank read latency: " << bank_read_lat << " ns" << std::endl;
    std::cout << "Chip read latency: " << chip_read_lat << " ns" << std::endl;
    std::cout << "Inner-bank datapath: " << inner_bank_datapath << " ns" << std::endl;

    bool test1_pass = (subarray_read_lat > 0.0 && subarray_read_lat < 10.0 &&
                       bank_read_lat > subarray_read_lat &&
                       chip_read_lat > bank_read_lat &&
                       inner_bank_datapath > 0.0 && inner_bank_datapath < 10.0);
    printTestResult("SRAM inner-bank timing values", test1_pass);

    // Test 2: Check PIM support
    std::cout << "\n--- Test 2: PIM Support ---" << std::endl;
    bool bank_pim = sram->supportsBankPIM();
    bool subarray_pim = sram->supportsSubarrayPIM();

    std::cout << "Bank-level PIM: " << (bank_pim ? "Supported" : "Not supported") << std::endl;
    std::cout << "Subarray-level PIM: " << (subarray_pim ? "Supported" : "Not supported") << std::endl;

    bool test2_pass = (bank_pim && subarray_pim);
    printTestResult("SRAM PIM support", test2_pass);

    // Test 3: Energy values
    std::cout << "\n--- Test 3: Energy Values ---" << std::endl;
    double read_energy = sram->getReadEnergy();
    double write_energy = sram->getWriteEnergy();
    double leakage = sram->getLeakagePower();

    std::cout << "Read energy: " << read_energy << " nJ" << std::endl;
    std::cout << "Write energy: " << write_energy << " nJ" << std::endl;
    std::cout << "Leakage power: " << leakage << " W" << std::endl;

    bool test3_pass = (read_energy > 0.0 && write_energy > read_energy && leakage > 0.0);
    printTestResult("SRAM energy values", test3_pass);

    std::cout << "\nSRAM Model Tests: " << (test1_pass && test2_pass && test3_pass ? "ALL PASSED" : "SOME FAILED") << std::endl;
}

//=============================================================================
// STT-MRAM Model Tests
//=============================================================================

void testSTTMRAMModel() {
    printTestHeader("STT-MRAM Model with Inner-Bank Timing");

    auto mram = std::make_shared<STTMRAMModel>("dummy_config.yaml");
    mram->initialize();

    // Test 1: Check read/write asymmetry
    std::cout << "\n--- Test 1: Read/Write Asymmetry ---" << std::endl;
    double subarray_read = mram->getSubarrayReadLatency();
    double subarray_write = mram->getSubarrayWriteLatency();
    double inner_read = mram->getInnerBankReadLatency();
    double inner_write = mram->getInnerBankWriteLatency();

    std::cout << "Subarray read: " << subarray_read << " ns" << std::endl;
    std::cout << "Subarray write: " << subarray_write << " ns (MTJ switching!)" << std::endl;
    std::cout << "Inner-bank read: " << inner_read << " ns" << std::endl;
    std::cout << "Inner-bank write: " << inner_write << " ns" << std::endl;
    std::cout << "Write/Read ratio: " << (subarray_write / subarray_read) << "x" << std::endl;

    bool test1_pass = (subarray_write > subarray_read * 2.0 &&
                       inner_write > inner_read * 2.0);
    printTestResult("STT-MRAM write asymmetry (slow MTJ switching)", test1_pass);

    // Test 2: Endurance
    std::cout << "\n--- Test 2: Endurance ---" << std::endl;
    uint64_t endurance = mram->getEndurance();
    std::cout << "Endurance: " << endurance << " writes" << std::endl;

    bool test2_pass = (endurance >= 1e14);  // STT-MRAM has very high endurance
    printTestResult("STT-MRAM endurance (>= 1e14)", test2_pass);

    // Test 3: PIM support
    std::cout << "\n--- Test 3: PIM Support ---" << std::endl;
    bool bank_pim = mram->supportsBankPIM();
    bool subarray_pim = mram->supportsSubarrayPIM();
    std::cout << "Bank-level PIM: " << (bank_pim ? "Yes" : "No") << std::endl;
    std::cout << "Subarray-level PIM: " << (subarray_pim ? "Yes" : "No") << std::endl;

    bool test3_pass = subarray_pim;  // At least subarray PIM should work
    printTestResult("STT-MRAM PIM support", test3_pass);

    std::cout << "\nSTT-MRAM Model Tests: " << (test1_pass && test2_pass && test3_pass ? "ALL PASSED" : "SOME FAILED") << std::endl;
}

//=============================================================================
// PCM Model Tests
//=============================================================================

void testPCMModel() {
    printTestHeader("PCM Model with Inner-Bank Timing");

    auto pcm = std::make_shared<PCMModel>("dummy_config.yaml");
    pcm->initialize();

    // Test 1: Check VERY slow writes
    std::cout << "\n--- Test 1: Very Slow Writes (PCM Characteristic) ---" << std::endl;
    double subarray_read = pcm->getSubarrayReadLatency();
    double set_write = pcm->getSubarraySetWriteLatency();
    double reset_write = pcm->getSubarrayResetWriteLatency();

    std::cout << "Subarray read: " << subarray_read << " ns" << std::endl;
    std::cout << "Subarray SET write: " << set_write << " ns (crystallization - SLOW!)" << std::endl;
    std::cout << "Subarray RESET write: " << reset_write << " ns (amorphization)" << std::endl;
    std::cout << "SET/Read ratio: " << (set_write / subarray_read) << "x (should be >10x!)" << std::endl;

    bool test1_pass = (set_write > subarray_read * 10.0 &&  // SET is VERY slow
                       reset_write > subarray_read * 3.0 &&  // RESET is faster than SET
                       set_write > reset_write);             // SET > RESET
    printTestResult("PCM very slow writes (SET >10x read)", test1_pass);

    // Test 2: Limited endurance
    std::cout << "\n--- Test 2: Limited Endurance ---" << std::endl;
    uint64_t endurance = pcm->getEndurance();
    std::cout << "Endurance: " << endurance << " writes" << std::endl;

    bool test2_pass = (endurance >= 1e8 && endurance <= 1e9);
    printTestResult("PCM limited endurance (1e8-1e9)", test2_pass);

    // Test 3: Read-only PIM suitability
    std::cout << "\n--- Test 3: Read-Only PIM Suitability ---" << std::endl;
    bool read_only = pcm->isReadOnlyPIM();
    std::cout << "Read-only PIM: " << (read_only ? "Yes (CORRECT!)" : "No") << std::endl;

    bool test3_pass = read_only;
    printTestResult("PCM read-only PIM", test3_pass);

    std::cout << "\nPCM Model Tests: " << (test1_pass && test2_pass && test3_pass ? "ALL PASSED" : "SOME FAILED") << std::endl;
}

//=============================================================================
// ReRAM Model Tests
//=============================================================================

void testReRAMModel() {
    printTestHeader("ReRAM Model with Analog Compute Support");

    auto reram = std::make_shared<ReRAMModel>("dummy_config.yaml");
    reram->initialize();

    // Test 1: Check analog compute support
    std::cout << "\n--- Test 1: Analog Compute Support ---" << std::endl;
    bool analog_capable = reram->supportsAnalogCompute();
    double analog_latency = reram->getAnalogComputeLatency();
    double analog_energy = reram->getAnalogComputeEnergy();

    std::cout << "Analog compute capable: " << (analog_capable ? "YES!" : "No") << std::endl;
    std::cout << "Analog compute latency: " << analog_latency << " ns (VERY fast!)" << std::endl;
    std::cout << "Analog compute energy: " << analog_energy << " pJ (very low!)" << std::endl;

    bool test1_pass = (analog_capable && analog_latency > 0.0 && analog_latency < 10.0);
    printTestResult("ReRAM analog compute support", test1_pass);

    // Test 2: Fast writes
    std::cout << "\n--- Test 2: Fast Writes (ReRAM Advantage) ---" << std::endl;
    double read_lat = reram->getSubarrayReadLatency();
    double write_lat = reram->getSubarrayWriteLatency();

    std::cout << "Subarray read: " << read_lat << " ns" << std::endl;
    std::cout << "Subarray write: " << write_lat << " ns (fast!)" << std::endl;
    std::cout << "Write/Read ratio: " << (write_lat / read_lat) << "x (should be <5x)" << std::endl;

    bool test2_pass = (write_lat < read_lat * 5.0);  // ReRAM has fast writes!
    printTestResult("ReRAM fast writes (<5x read)", test2_pass);

    // Test 3: Good endurance
    std::cout << "\n--- Test 3: Good Endurance ---" << std::endl;
    uint64_t endurance = reram->getEndurance();
    std::cout << "Endurance: " << endurance << " writes" << std::endl;

    bool test3_pass = (endurance >= 1e10);  // ReRAM has good endurance
    printTestResult("ReRAM good endurance (>= 1e10)", test3_pass);

    // Test 4: Analog compute vs regular compute
    std::cout << "\n--- Test 4: Analog vs Regular Compute Comparison ---" << std::endl;
    double regular_compute = read_lat + write_lat;
    double speedup = regular_compute / analog_latency;
    std::cout << "Regular compute (R+W): " << regular_compute << " ns" << std::endl;
    std::cout << "Analog compute: " << analog_latency << " ns" << std::endl;
    std::cout << "Analog speedup: " << speedup << "x" << std::endl;

    bool test4_pass = (speedup > 2.0);  // Analog should be faster
    printTestResult("ReRAM analog compute speedup", test4_pass);

    std::cout << "\nReRAM Model Tests: "
              << (test1_pass && test2_pass && test3_pass && test4_pass ? "ALL PASSED" : "SOME FAILED")
              << std::endl;
}

//=============================================================================
// Cross-Technology Comparison Tests
//=============================================================================

void testCrossTechnologyComparison() {
    printTestHeader("Cross-Technology Comparison");

    auto sram = std::make_shared<SRAMModel>("dummy_config.yaml");
    auto mram = std::make_shared<STTMRAMModel>("dummy_config.yaml");
    auto pcm = std::make_shared<PCMModel>("dummy_config.yaml");
    auto reram = std::make_shared<ReRAMModel>("dummy_config.yaml");

    sram->initialize();
    mram->initialize();
    pcm->initialize();
    reram->initialize();

    std::cout << "\n--- Inner-Bank Read Latency Comparison ---" << std::endl;
    std::cout << std::setw(15) << "Technology" << std::setw(20) << "Inner-Bank (ns)" << std::endl;
    std::cout << std::string(35, '-') << std::endl;
    std::cout << std::setw(15) << "SRAM" << std::setw(20) << sram->getInnerBankDatapathLatency() << std::endl;
    std::cout << std::setw(15) << "STT-MRAM" << std::setw(20) << mram->getInnerBankReadLatency() << std::endl;
    std::cout << std::setw(15) << "PCM" << std::setw(20) << pcm->getInnerBankReadLatency() << std::endl;
    std::cout << std::setw(15) << "ReRAM" << std::setw(20) << reram->getInnerBankReadLatency() << std::endl;

    std::cout << "\n--- PIM Suitability Comparison ---" << std::endl;
    std::cout << std::setw(15) << "Technology" << std::setw(15) << "Bank PIM" << std::setw(20) << "Subarray PIM" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    std::cout << std::setw(15) << "SRAM"
              << std::setw(15) << (sram->supportsBankPIM() ? "Yes" : "No")
              << std::setw(20) << (sram->supportsSubarrayPIM() ? "Yes" : "No") << std::endl;
    std::cout << std::setw(15) << "STT-MRAM"
              << std::setw(15) << (mram->supportsBankPIM() ? "Yes" : "No")
              << std::setw(20) << (mram->supportsSubarrayPIM() ? "Yes" : "No") << std::endl;
    std::cout << std::setw(15) << "PCM"
              << std::setw(15) << (pcm->supportsBankPIM() ? "Yes" : "No")
              << std::setw(20) << (pcm->supportsSubarrayPIM() ? "Yes (R/O)" : "No") << std::endl;
    std::cout << std::setw(15) << "ReRAM"
              << std::setw(15) << (reram->supportsBankPIM() ? "Yes" : "No")
              << std::setw(20) << (reram->supportsSubarrayPIM() ? "Yes (Analog!)" : "No") << std::endl;

    std::cout << "\n--- Special Features ---" << std::endl;
    std::cout << "SRAM:     Fastest read latency (2-4ns)" << std::endl;
    std::cout << "STT-MRAM: High endurance (>1e14), non-volatile" << std::endl;
    std::cout << "PCM:      Read-only PIM (very slow writes)" << std::endl;
    std::cout << "ReRAM:    ANALOG COMPUTE support (unique!)" << std::endl;

    std::cout << "\nCross-Technology Comparison: COMPLETED" << std::endl;
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Memory Models Inner-Bank Timing Test Suite" << std::endl;
    std::cout << "Testing: SRAM, STT-MRAM, PCM, ReRAM" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        testSRAMModel();
        testSTTMRAMModel();
        testPCMModel();
        testReRAMModel();
        testCrossTechnologyComparison();

        std::cout << "\n========================================" << std::endl;
        std::cout << "ALL TEST SUITES COMPLETED!" << std::endl;
        std::cout << "========================================" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << std::endl;
        return 1;
    }
}
