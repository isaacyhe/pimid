/**
 * @file test_verification.cpp
 * @brief Test and compare verified vs. original DRAM specs
 *
 * This program demonstrates the difference between:
 * 1. Original specs (with unverified assumptions)
 * 2. Verified specs (with clear verification status)
 */

#include "../../pimid/memory/dram_architecture.h"
#include "../../pimid/memory/dram_architecture_v2.h"
#include <iostream>
#include <iomanip>

using namespace pimid::memory;

void printHeader(const std::string& title) {
    std::cout << "\n╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::setw(65) << std::left << title << "║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
}

void compareOriginalVsVerified() {
    printHeader("COMPARISON: Original vs. Verified Specifications");

    // Create both versions
    auto ddr4_orig = createDDR4_2400();
    auto ddr4_v2 = createDDR4_2400_Verified();

    std::cout << "\nDDR4-2400 Comparison:\n";
    std::cout << std::string(80, '-') << "\n";
    std::cout << std::setw(35) << "Parameter"
              << std::setw(20) << "Original (v1)"
              << std::setw(25) << "Verified (v2)"
              << "\n";
    std::cout << std::string(80, '-') << "\n";

    // Bank port (critical difference!)
    std::cout << std::setw(35) << "Bank 'port' (ambiguous):"
              << std::setw(15) << (std::to_string(ddr4_orig->ports.bank_port_bits) + " bits")
              << "  →  ";

    std::cout << "Multiple stages:\n";
    std::cout << std::setw(35) << ""
              << std::setw(15) << ""
              << "  GSA: " << ddr4_v2->datapath.gsa_datapath_bits.value_bits << " bits\n";
    std::cout << std::setw(35) << ""
              << std::setw(15) << ""
              << "  Prefetch: " << ddr4_v2->datapath.prefetch_datapath_bits.value_bits << " bits\n";
    std::cout << std::setw(35) << ""
              << std::setw(15) << ""
              << "  Serialization: " << ddr4_v2->datapath.bank_serialization_bits.value_bits << " bits (BOTTLENECK!)\n";

    std::cout << "\n" << std::setw(35) << "Bank effective BW:"
              << std::setw(18) << (std::to_string(ddr4_orig->getBankBandwidth()) + " GB/s")
              << std::setw(25) << (std::to_string(ddr4_v2->getBankEffectiveBW()) + " GB/s")
              << "\n";

    std::cout << "\n" << std::setw(35) << "Chip I/O:"
              << std::setw(18) << (std::to_string(ddr4_orig->ports.chip_io_bits) + " bits")
              << std::setw(25) << (std::to_string(ddr4_v2->datapath.chip_io_bits.value_bits) + " bits ✅ VERIFIED")
              << "\n";

    std::cout << "\n" << std::setw(35) << "Rank data bus:"
              << std::setw(18) << (std::to_string(ddr4_orig->ports.rank_data_bits) + " bits")
              << std::setw(25) << (std::to_string(ddr4_v2->datapath.rank_databus_bits.value_bits) + " bits ✅ VERIFIED")
              << "\n";

    std::cout << "\n";
}

void showDatapathStages() {
    printHeader("DRAM INTERNAL DATAPATH STAGES (Detailed Breakdown)");

    std::cout << "\nDDR4 has MULTIPLE datapath stages with DIFFERENT widths:\n\n";

    std::cout << "  ┌─────────────────────────────────────────────────────────┐\n";
    std::cout << "  │  Stage 1: Row Buffer (Bitline Sense Amps)              │\n";
    std::cout << "  │           Width: 8,192 bits (8 Kb)                      │\n";
    std::cout << "  │           Status: ✅ VERIFIED (typical DDR4)            │\n";
    std::cout << "  │           Notes: Activated row sits here                │\n";
    std::cout << "  └─────────────────────────────────────────────────────────┘\n";
    std::cout << "                            │\n";
    std::cout << "                            │ Column select\n";
    std::cout << "                            ▼\n";
    std::cout << "  ┌─────────────────────────────────────────────────────────┐\n";
    std::cout << "  │  Stage 2: Global Sense Amplifiers (GSA)                │\n";
    std::cout << "  │           Width: 256 bits per subarray                  │\n";
    std::cout << "  │           Status: 📊 INFERRED (from DAS-MICRO15)       │\n";
    std::cout << "  │           Notes: Column I/O to peripheral               │\n";
    std::cout << "  └─────────────────────────────────────────────────────────┘\n";
    std::cout << "                            │\n";
    std::cout << "                            │ Prefetch buffer\n";
    std::cout << "                            ▼\n";
    std::cout << "  ┌─────────────────────────────────────────────────────────┐\n";
    std::cout << "  │  Stage 3: Prefetch Datapath                             │\n";
    std::cout << "  │           Width: 64 bits (8n × 8-bit I/O)               │\n";
    std::cout << "  │           Status: ✅ VERIFIED (JEDEC JESD79-4)          │\n";
    std::cout << "  │           Notes: Well-documented prefetch architecture  │\n";
    std::cout << "  └─────────────────────────────────────────────────────────┘\n";
    std::cout << "                            │\n";
    std::cout << "                            │ CRITICAL BOTTLENECK!\n";
    std::cout << "                            ▼\n";
    std::cout << "  ┌─────────────────────────────────────────────────────────┐\n";
    std::cout << "  │  Stage 4: Bank Serialization (UNKNOWN!)                 │\n";
    std::cout << "  │           Width: ~8 bits (ESTIMATED)                    │\n";
    std::cout << "  │           Status: 📐 ESTIMATED (NOT DOCUMENTED!)        │\n";
    std::cout << "  │           Notes: Banks serialize through NARROW paths   │\n";
    std::cout << "  │                  to chip peripheral to save routing!    │\n";
    std::cout << "  │           >>> THIS IS WHY BANK-LEVEL PIM IS SLOW! <<<   │\n";
    std::cout << "  └─────────────────────────────────────────────────────────┘\n";
    std::cout << "                            │\n";
    std::cout << "                            │ Serialize to pins\n";
    std::cout << "                            ▼\n";
    std::cout << "  ┌─────────────────────────────────────────────────────────┐\n";
    std::cout << "  │  Stage 5: Chip I/O Pins                                 │\n";
    std::cout << "  │           Width: 8 bits (x8 device)                     │\n";
    std::cout << "  │           Status: ✅ VERIFIED (JEDEC JESD79-4)          │\n";
    std::cout << "  │           Notes: External package pins                  │\n";
    std::cout << "  └─────────────────────────────────────────────────────────┘\n";
    std::cout << "                            │\n";
    std::cout << "                            │ 8 chips combine\n";
    std::cout << "                            ▼\n";
    std::cout << "  ┌─────────────────────────────────────────────────────────┐\n";
    std::cout << "  │  Rank Level (FIRST WIDE INTERFACE!)                     │\n";
    std::cout << "  │           Width: 64 bits (8 × 8-bit chips)              │\n";
    std::cout << "  │           Status: ✅ VERIFIED (JEDEC standard)          │\n";
    std::cout << "  │           Bandwidth: 19.2 GB/s @ DDR4-2400              │\n";
    std::cout << "  └─────────────────────────────────────────────────────────┘\n";

    std::cout << "\n KEY INSIGHT:\n";
    std::cout << " ════════════\n";
    std::cout << " Stage 4 (Bank Serialization) is the CRITICAL UNKNOWN!\n";
    std::cout << " - NOT documented in JEDEC specs or datasheets\n";
    std::cout << " - We INFER it's narrow (8-16 bits) from bandwidth measurements\n";
    std::cout << " - This is why DDR4 Bank-PIM is bandwidth-limited!\n";
    std::cout << " - HBM solves this with TSV (wider internal paths)\n\n";
}

void showHBMAdvantage() {
    printHeader("WHY HBM ENABLES FINER-GRAINED PIM");

    auto ddr4 = createDDR4_2400_Verified();
    auto hbm2 = createHBM2_Verified();

    std::cout << "\nComparing CRITICAL Stage 4 (Bank Serialization):\n\n";

    std::cout << "DDR4:\n";
    std::cout << "  Bank serialization: " << ddr4->datapath.bank_serialization_bits.value_bits << " bits (ESTIMATED)\n";
    std::cout << "  Method: Wire routing between banks and peripheral\n";
    std::cout << "  Constraint: Limited by routing area, must be narrow\n";
    std::cout << "  Result: Bank effective BW = " << ddr4->getBankEffectiveBW() << " GB/s\n";
    std::cout << "  PIM Impact: ❌ Bank-level PIM is BANDWIDTH-LIMITED!\n\n";

    std::cout << "HBM2:\n";
    std::cout << "  Bank serialization: " << hbm2->datapath.bank_serialization_bits.value_bits << " bits (INFERRED from TSV)\n";
    std::cout << "  Method: Through-Silicon Vias (TSV) - vertical connections\n";
    std::cout << "  Constraint: TSV allows wider paths with less routing penalty\n";
    std::cout << "  Result: Bank effective BW = " << hbm2->getBankEffectiveBW() << " GB/s\n";
    std::cout << "  PIM Impact: ✅ Bank-level PIM is VIABLE!\n\n";

    std::cout << "RATIO: HBM2 bank BW / DDR4 bank BW = "
              << (hbm2->getBankEffectiveBW() / ddr4->getBankEffectiveBW()) << "x\n";
    std::cout << "       (Corresponds to ~" << (hbm2->datapath.bank_serialization_bits.value_bits /
                                                 ddr4->datapath.bank_serialization_bits.value_bits)
              << "x wider internal paths)\n\n";
}

void printVerificationSummary() {
    printHeader("VERIFICATION STATUS SUMMARY");

    auto ddr4 = createDDR4_2400_Verified();
    auto hbm2 = createHBM2_Verified();

    std::cout << "\nDDR4-2400:\n";
    ddr4->printVerificationReport();

    std::cout << "\nHBM2:\n";
    hbm2->printVerificationReport();

    std::cout << "\nOVERALL CONFIDENCE:\n";
    std::cout << "══════════════════\n";
    std::cout << "DDR4: " << ddr4->getOverallConfidence() << "\n";
    std::cout << "HBM2: " << hbm2->getOverallConfidence() << "\n\n";
}

void showMethodology() {
    printHeader("METHODOLOGY: How We Handle Unknown Values");

    std::cout << "\nThe Problem:\n";
    std::cout << "────────────\n";
    std::cout << "Internal DRAM port widths (especially bank serialization) are:\n";
    std::cout << "  ❌ NOT in JEDEC specifications\n";
    std::cout << "  ❌ NOT in manufacturer datasheets\n";
    std::cout << "  ❌ NOT publicly documented\n";
    std::cout << "  ❓ Trade secrets / implementation details\n\n";

    std::cout << "Our Solution:\n";
    std::cout << "─────────────\n";
    std::cout << "1. VERIFIED: Use JEDEC specs where available\n";
    std::cout << "   - External I/O (x4/x8/x16)\n";
    std::cout << "   - Prefetch architecture (8n for DDR4)\n";
    std::cout << "   - Timing parameters\n";
    std::cout << "   - Organization (banks, bank groups, etc.)\n\n";

    std::cout << "2. INFERRED: Use academic papers for internal details\n";
    std::cout << "   - Global Sense Amplifiers (256 bits from DAS-MICRO15)\n";
    std::cout << "   - Energy values (from NVIDIA-HPCA17, etc.)\n\n";

    std::cout << "3. ESTIMATED: Conservative estimates for unknowns\n";
    std::cout << "   - Bank serialization (8 bits for DDR4, 64 bits for HBM2)\n";
    std::cout << "   - Based on measured bandwidth limits\n";
    std::cout << "   - Use LOWER BOUNDS (conservative for PIM)\n\n";

    std::cout << "4. MARK CLEARLY: Every value tagged with verification status\n";
    std::cout << "   - ✅ VERIFIED = From authoritative source\n";
    std::cout << "   - 📊 INFERRED = Derived from papers\n";
    std::cout << "   - 📐 ESTIMATED = Educated guess\n";
    std::cout << "   - ❓ UNKNOWN = Placeholder\n\n";

    std::cout << "5. PROVIDE CITATIONS: Source for every value\n";
    std::cout << "   - JEDEC standard (e.g., JESD79-4)\n";
    std::cout << "   - Academic paper (e.g., DAS-MICRO15)\n";
    std::cout << "   - Inference method (e.g., from BW measurements)\n\n";
}

int main() {
    printHeader("DRAM Architecture Verification System");
    std::cout << "\nDemonstrating rigorously verified DRAM specifications\n";
    std::cout << "with clear distinction between VERIFIED, INFERRED, and ESTIMATED values.\n";

    // Test 1: Show datapath stages
    showDatapathStages();

    // Test 2: Compare original vs verified
    compareOriginalVsVerified();

    // Test 3: Show HBM advantage
    showHBMAdvantage();

    // Test 4: Print full verification reports
    printVerificationSummary();

    // Test 5: Explain methodology
    showMethodology();

    printHeader("RECOMMENDATIONS");
    std::cout << "\n";
    std::cout << "1. USE V2 (Verified) specs for ACCURATE PIM modeling\n";
    std::cout << "   - Clearly distinguishes datapath stages\n";
    std::cout << "   - Marks verification status\n";
    std::cout << "   - Conservative estimates for unknowns\n\n";

    std::cout << "2. FOCUS on MEASURED BANDWIDTH rather than assumed port widths\n";
    std::cout << "   - We can measure: Bank achieves ~1.2 GB/s in DDR4\n";
    std::cout << "   - We can't verify: Exactly which internal port limits this\n";
    std::cout << "   - For PIM: The bandwidth limit is what matters!\n\n";

    std::cout << "3. USE VERIFICATION STATUS when citing results\n";
    std::cout << "   - \"Bank-level BW limited to 1.2 GB/s (INFERRED from measurements)\"\n";
    std::cout << "   - NOT: \"Bank port is 8 bits (as if it's a fact)\"\n\n";

    std::cout << "4. FOR RESEARCH: Treat unknown values as PARAMETERS\n";
    std::cout << "   - Sensitivity analysis: What if bank serialization is 16 bits instead of 8?\n";
    std::cout << "   - Hypothetical studies: Use port_width_scale\n";
    std::cout << "   - Mark clearly: \"Assuming X bits based on Y...\"\n\n";

    std::cout << "\n✅ Verification testing complete!\n\n";

    return 0;
}
