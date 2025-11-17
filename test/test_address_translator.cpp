/**
 * @file test_address_translator.cpp
 * @brief Comprehensive tests for AddressTranslator implementation
 */

#include "address_translation/address_translator.h"
#include <iostream>
#include <cassert>
#include <vector>

using namespace pimid;

struct TestResult {
    std::string test_name;
    bool passed;
    std::string message;
};

std::vector<TestResult> test_results;

void reportTest(const std::string& name, bool passed, const std::string& msg = "") {
    test_results.push_back({name, passed, msg});
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << name;
    if (!msg.empty()) {
        std::cout << ": " << msg;
    }
    std::cout << std::endl;
}

//=============================================================================
// Address Translator Tests
//=============================================================================

void testAddressTranslatorConstruction() {
    std::cout << "\n=== Testing Address Translator Construction ===" << std::endl;

    AddressTranslationConfig config;
    config.page_size_bytes = 4096;
    config.page_bits = 12;
    config.tlb_entries = 64;
    config.tlb_associativity = 4;
    config.tlb_hit_latency = 1;
    config.page_walk_latency = 20;

    AddressTranslator translator(config);

    reportTest("AddressTranslator: Construction", true,
               "Translator created successfully");
}

void testTLBHit() {
    std::cout << "\n=== Testing TLB Hit Behavior ===" << std::endl;

    AddressTranslationConfig config;
    config.page_size_bytes = 4096;
    config.page_bits = 12;
    config.tlb_entries = 64;
    config.tlb_associativity = 4;
    config.tlb_hit_latency = 1;
    config.page_walk_latency = 20;

    AddressTranslator translator(config);

    // Map a page
    Address virtual_page = 0x1000;  // Page number (address >> 12)
    Address physical_page = 0x5000;
    uint32_t pe_id = 0;

    translator.mapPage(virtual_page, physical_page, pe_id);

    // First access - should be TLB miss, then populate TLB
    Address virtual_addr = (virtual_page << 12) | 0x123; // Add offset
    bool hit = false;
    Cycle latency = 0;

    Address physical_addr = translator.translate(virtual_addr, pe_id, hit, latency);

    reportTest("AddressTranslator: First Access (TLB Miss)", !hit,
               "First access should miss TLB (latency=" + std::to_string(latency) + ")");

    // Second access to same page - should be TLB hit
    Address virtual_addr2 = (virtual_page << 12) | 0x456;
    bool hit2 = false;
    Cycle latency2 = 0;

    Address physical_addr2 = translator.translate(virtual_addr2, pe_id, hit2, latency2);

    reportTest("AddressTranslator: Second Access (TLB Hit)", hit2,
               "Second access should hit TLB (latency=" + std::to_string(latency2) + ")");

    reportTest("AddressTranslator: TLB Hit Latency", latency2 == config.tlb_hit_latency,
               "TLB hit should have 1 cycle latency");

    // Verify address translation correctness
    Address expected_physical = (physical_page << 12) | 0x456;
    reportTest("AddressTranslator: Correct Translation", physical_addr2 == expected_physical,
               "Physical address should match mapped page");
}

void testPageTableWalk() {
    std::cout << "\n=== Testing Page Table Walk ===" << std::endl;

    AddressTranslationConfig config;
    config.page_size_bytes = 4096;
    config.page_bits = 12;
    config.tlb_entries = 4; // Small TLB to force evictions
    config.tlb_associativity = 2;
    config.tlb_hit_latency = 1;
    config.page_walk_latency = 20;

    AddressTranslator translator(config);
    uint32_t pe_id = 0;

    // Map multiple pages
    for (uint64_t i = 0; i < 10; i++) {
        Address virt_page = 0x1000 + i;
        Address phys_page = 0x5000 + i;
        translator.mapPage(virt_page, phys_page, pe_id);
    }

    // Access all pages (will overflow TLB)
    int tlb_misses = 0;
    for (uint64_t i = 0; i < 10; i++) {
        Address virt_addr = ((0x1000 + i) << 12) | 0x100;
        bool hit = false;
        Cycle latency = 0;
        translator.translate(virt_addr, pe_id, hit, latency);
        if (!hit) tlb_misses++;
    }

    reportTest("AddressTranslator: Page Table Walk", tlb_misses > 0,
               "Should have " + std::to_string(tlb_misses) + " TLB misses with small TLB");

    // Get statistics
    auto stats = translator.getStats(pe_id);
    reportTest("AddressTranslator: Statistics Tracking",
               stats.total_translations == 10 && stats.tlb_misses == tlb_misses,
               "Stats: " + std::to_string(stats.tlb_hits) + " hits, " +
               std::to_string(stats.tlb_misses) + " misses");
}

void testTLBInvalidation() {
    std::cout << "\n=== Testing TLB Invalidation ===" << std::endl;

    AddressTranslationConfig config;
    config.page_size_bytes = 4096;
    config.page_bits = 12;
    config.tlb_entries = 64;
    config.tlb_associativity = 4;
    config.tlb_hit_latency = 1;
    config.page_walk_latency = 20;

    AddressTranslator translator(config);
    uint32_t pe_id = 0;

    // Map and access a page to populate TLB
    Address virt_page = 0x1000;
    Address phys_page = 0x5000;
    translator.mapPage(virt_page, phys_page, pe_id);

    bool hit1 = false;
    Cycle lat1 = 0;
    translator.translate((virt_page << 12), pe_id, hit1, lat1);

    // Second access should hit TLB
    bool hit2 = false;
    Cycle lat2 = 0;
    translator.translate((virt_page << 12), pe_id, hit2, lat2);

    reportTest("AddressTranslator: TLB Populated", hit2,
               "TLB should be populated before invalidation");

    // Invalidate TLB for this PE
    translator.invalidateTLB(pe_id);

    // Access again - should miss TLB
    bool hit3 = false;
    Cycle lat3 = 0;
    translator.translate((virt_page << 12), pe_id, hit3, lat3);

    reportTest("AddressTranslator: TLB Invalidation", !hit3,
               "TLB should miss after invalidation");
}

void testPageFaultHandling() {
    std::cout << "\n=== Testing Page Fault Handling ===" << std::endl;

    AddressTranslationConfig config;
    config.page_size_bytes = 4096;
    config.page_bits = 12;
    config.tlb_entries = 64;
    config.tlb_associativity = 4;
    config.tlb_hit_latency = 1;
    config.page_walk_latency = 20;

    AddressTranslator translator(config);
    uint32_t pe_id = 0;

    // Access unmapped page - should trigger page fault handler
    Address unmapped_addr = 0x9999 << 12;
    bool hit = false;
    Cycle latency = 0;

    Address result = translator.translate(unmapped_addr, pe_id, hit, latency);

    reportTest("AddressTranslator: Page Fault Handling", !hit,
               "Unmapped page should cause page fault (latency=" + std::to_string(latency) + ")");

    // Verify identity mapping was created
    Address expected = (0x9999 << 12);
    reportTest("AddressTranslator: Identity Mapping", result == expected,
               "Page fault should create identity mapping");

    // Second access should hit (fault handler populated TLB)
    bool hit2 = false;
    Cycle lat2 = 0;
    Address result2 = translator.translate(unmapped_addr, pe_id, hit2, lat2);

    reportTest("AddressTranslator: Page Fault Recovery", hit2,
               "Second access after fault should hit TLB");
}

void testMultiPEIsolation() {
    std::cout << "\n=== Testing Multi-PE TLB Isolation ===" << std::endl;

    AddressTranslationConfig config;
    config.page_size_bytes = 4096;
    config.page_bits = 12;
    config.tlb_entries = 64;
    config.tlb_associativity = 4;
    config.tlb_hit_latency = 1;
    config.page_walk_latency = 20;

    AddressTranslator translator(config);

    // Map same virtual page to different physical pages for different PEs
    Address virt_page = 0x1000;
    translator.mapPage(virt_page, 0x5000, 0); // PE 0
    translator.mapPage(virt_page, 0x6000, 1); // PE 1

    // Access from PE 0
    bool hit0 = false;
    Cycle lat0 = 0;
    Address phys0 = translator.translate((virt_page << 12), 0, hit0, lat0);

    // Access from PE 1
    bool hit1 = false;
    Cycle lat1 = 0;
    Address phys1 = translator.translate((virt_page << 12), 1, hit1, lat1);

    Address expected0 = 0x5000 << 12;
    Address expected1 = 0x6000 << 12;

    reportTest("AddressTranslator: Multi-PE Isolation",
               phys0 == expected0 && phys1 == expected1,
               "Different PEs should get different physical addresses");
}

void testTLBStatistics() {
    std::cout << "\n=== Testing TLB Statistics ===" << std::endl;

    AddressTranslationConfig config;
    config.page_size_bytes = 4096;
    config.page_bits = 12;
    config.tlb_entries = 64;
    config.tlb_associativity = 4;
    config.tlb_hit_latency = 1;
    config.page_walk_latency = 20;

    AddressTranslator translator(config);
    uint32_t pe_id = 0;

    // Map 5 pages
    for (uint64_t i = 0; i < 5; i++) {
        translator.mapPage(0x1000 + i, 0x5000 + i, pe_id);
    }

    // Access pattern: 0,1,2,3,4,0,1,2,3,4 (5 misses, 5 hits)
    for (int round = 0; round < 2; round++) {
        for (uint64_t i = 0; i < 5; i++) {
            bool hit = false;
            Cycle lat = 0;
            translator.translate(((0x1000 + i) << 12), pe_id, hit, lat);
        }
    }

    auto stats = translator.getStats(pe_id);

    reportTest("AddressTranslator: Total Translations", stats.total_translations == 10,
               "Should have 10 total translations");

    reportTest("AddressTranslator: TLB Hits", stats.tlb_hits == 5,
               "Should have 5 TLB hits (got " + std::to_string(stats.tlb_hits) + ")");

    reportTest("AddressTranslator: TLB Misses", stats.tlb_misses == 5,
               "Should have 5 TLB misses (got " + std::to_string(stats.tlb_misses) + ")");

    double hit_rate = (stats.tlb_hits * 100.0) / stats.total_translations;
    reportTest("AddressTranslator: 50% Hit Rate", std::abs(hit_rate - 50.0) < 0.1,
               "Hit rate = " + std::to_string(hit_rate) + "%");
}

//=============================================================================
// Main Test Driver
//=============================================================================

int main() {
    std::cout << "╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  PIMID Address Translator Test Suite                  ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;

    // Run all tests
    testAddressTranslatorConstruction();
    testTLBHit();
    testPageTableWalk();
    testTLBInvalidation();
    testPageFaultHandling();
    testMultiPEIsolation();
    testTLBStatistics();

    // Print summary
    std::cout << "\n╔════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║  Test Summary                                          ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════╝" << std::endl;

    int passed = 0, failed = 0;
    for (const auto& result : test_results) {
        if (result.passed) passed++;
        else failed++;
    }

    std::cout << "Total Tests:  " << test_results.size() << std::endl;
    std::cout << "Passed:       " << passed << " ✓" << std::endl;
    std::cout << "Failed:       " << failed << (failed > 0 ? " ✗" : "") << std::endl;
    std::cout << "Success Rate: " << (100.0 * passed / test_results.size()) << "%" << std::endl;

    if (failed > 0) {
        std::cout << "\nFailed Tests:" << std::endl;
        for (const auto& result : test_results) {
            if (!result.passed) {
                std::cout << "  - " << result.test_name << std::endl;
                if (!result.message.empty()) {
                    std::cout << "    " << result.message << std::endl;
                }
            }
        }
    }

    // Print detailed statistics
    std::cout << "\nDetailed TLB Statistics:" << std::endl;
    AddressTranslationConfig config;
    config.page_size_bytes = 4096;
    config.page_bits = 12;
    config.tlb_entries = 64;
    config.tlb_associativity = 4;
    config.tlb_hit_latency = 1;
    config.page_walk_latency = 20;

    AddressTranslator translator(config);
    translator.printStats();

    return (failed == 0) ? 0 : 1;
}
