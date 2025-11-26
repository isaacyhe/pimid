/**
 * @file test_error_handling.cpp
 * @brief Test error handling for memory network creation
 *
 * This test verifies that proper exceptions are thrown for invalid inputs
 */

#include "internal_memory_network.h"
#include <iostream>
#include <cassert>

using namespace pimid;

// Test counter
int tests_passed = 0;
int tests_failed = 0;

void testInvalidInputs() {
    std::cout << "\n╔══════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║         Error Handling Validation Tests             ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════╝\n" << std::endl;

    // Test 1: Empty memory technology
    std::cout << "[Test 1] Empty memory technology string..." << std::endl;
    try {
        auto network = createInternalMemoryNetwork("", 16, 8, 2, 1);
        std::cerr << "❌ FAILED: Should have thrown exception for empty technology" << std::endl;
        tests_failed++;
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ PASSED: Caught exception: " << e.what() << std::endl;
        tests_passed++;
    }

    // Test 2: Unknown memory technology
    std::cout << "\n[Test 2] Unknown memory technology..." << std::endl;
    try {
        auto network = createInternalMemoryNetwork("INVALID_TECH", 16, 8, 2, 1);
        std::cerr << "❌ FAILED: Should have thrown exception for unknown technology" << std::endl;
        tests_failed++;
    } catch (const std::runtime_error& e) {
        std::cout << "✅ PASSED: Caught exception: " << e.what() << std::endl;
        tests_passed++;
    }

    // Test 3: Negative number of subarrays
    std::cout << "\n[Test 3] Negative number of subarrays..." << std::endl;
    try {
        auto network = createInternalMemoryNetwork("SRAM", -5, 8, 2, 1);
        std::cerr << "❌ FAILED: Should have thrown exception for negative subarrays" << std::endl;
        tests_failed++;
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ PASSED: Caught exception: " << e.what() << std::endl;
        tests_passed++;
    }

    // Test 4: Zero banks
    std::cout << "\n[Test 4] Zero banks..." << std::endl;
    try {
        auto network = createInternalMemoryNetwork("SRAM", 16, 0, 2, 1);
        std::cerr << "❌ FAILED: Should have thrown exception for zero banks" << std::endl;
        tests_failed++;
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ PASSED: Caught exception: " << e.what() << std::endl;
        tests_passed++;
    }

    // Test 5: Excessive number of subarrays
    std::cout << "\n[Test 5] Excessive number of subarrays..." << std::endl;
    try {
        auto network = createInternalMemoryNetwork("SRAM", 2048, 8, 2, 1);
        std::cerr << "❌ FAILED: Should have thrown exception for excessive subarrays" << std::endl;
        tests_failed++;
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ PASSED: Caught exception: " << e.what() << std::endl;
        tests_passed++;
    }

    // Test 6: Banks not divisible by bank groups
    std::cout << "\n[Test 6] Banks not evenly divisible by bank groups..." << std::endl;
    try {
        auto network = createInternalMemoryNetwork("SRAM", 16, 9, 2, 1);
        std::cerr << "❌ FAILED: Should have thrown exception for misaligned banks/BGs" << std::endl;
        tests_failed++;
    } catch (const std::invalid_argument& e) {
        std::cout << "✅ PASSED: Caught exception: " << e.what() << std::endl;
        tests_passed++;
    }

    // Test 7: Valid SRAM configuration (should succeed)
    std::cout << "\n[Test 7] Valid SRAM configuration (should succeed)..." << std::endl;
    try {
        auto network = createInternalMemoryNetwork("SRAM", 16, 8, 2, 1);
        std::cout << "✅ PASSED: Valid configuration accepted" << std::endl;
        tests_passed++;
    } catch (const std::exception& e) {
        std::cerr << "❌ FAILED: Valid configuration rejected: " << e.what() << std::endl;
        tests_failed++;
    }

    // Test 8: Valid STT-MRAM configuration
    std::cout << "\n[Test 8] Valid STT-MRAM configuration (should succeed)..." << std::endl;
    try {
        auto network = createInternalMemoryNetwork("STT-MRAM", 16, 8, 2, 1);
        std::cout << "✅ PASSED: Valid configuration accepted" << std::endl;
        tests_passed++;
    } catch (const std::exception& e) {
        std::cerr << "❌ FAILED: Valid configuration rejected: " << e.what() << std::endl;
        tests_failed++;
    }

    // Test 9: Valid PCM configuration
    std::cout << "\n[Test 9] Valid PCM configuration (should succeed)..." << std::endl;
    try {
        auto network = createInternalMemoryNetwork("PCM", 16, 8, 2, 1);
        std::cout << "✅ PASSED: Valid configuration accepted" << std::endl;
        tests_passed++;
    } catch (const std::exception& e) {
        std::cerr << "❌ FAILED: Valid configuration rejected: " << e.what() << std::endl;
        tests_failed++;
    }

    // Test 10: Valid ReRAM configuration
    std::cout << "\n[Test 10] Valid ReRAM configuration (should succeed)..." << std::endl;
    try {
        auto network = createInternalMemoryNetwork("ReRAM", 16, 8, 2, 1);
        std::cout << "✅ PASSED: Valid configuration accepted" << std::endl;
        tests_passed++;
    } catch (const std::exception& e) {
        std::cerr << "❌ FAILED: Valid configuration rejected: " << e.what() << std::endl;
        tests_failed++;
    }
}

int main() {
    testInvalidInputs();

    // Print summary
    std::cout << "\n╔══════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                  Test Summary                         ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "Total tests:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << "Passed:       " << tests_passed << std::endl;
    std::cout << "Failed:       " << tests_failed << std::endl;
    std::cout << "══════════════════════════════════════════════════════" << std::endl;

    if (tests_failed == 0) {
        std::cout << "\n✅ ALL ERROR HANDLING TESTS PASSED!\n" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ SOME TESTS FAILED\n" << std::endl;
        return 1;
    }
}
