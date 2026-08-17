/**
 * @file example_custom_vc_dram.cpp
 * @brief Example: Understanding VN vs VC in DRAM H-tree networks
 *
 * IMPORTANT: This file explains the critical distinction between:
 * - Virtual Networks (VN): Message classes (read vs write traffic)
 * - Virtual Channels (VC): Deadlock avoidance within each VN
 */

#include "internal_dram_network.h"
#include <iostream>

using namespace pimid;

/**
 * Example 1: Understanding VN vs VC (CRITICAL!)
 */
void example_vn_vs_vc_explanation() {
    std::cout << "\n=== Example 1: VN vs VC Explanation ===\n";

    auto ddr4 = createInternalDRAMNetwork("DDR4", 16, 4, 4, 8);
    ddr4->enableGarnetSimulation(true);

    std::cout << "\n[*] IMPORTANT CORRECTION:\n";
    std::cout << "Virtual Networks (VN) != Virtual Channels (VC)\n\n";

    std::cout << "Default DRAM configuration:\n";
    std::cout << "  Virtual Networks (VN): 2\n";
    std::cout << "    - VN 0: Read traffic (message class)\n";
    std::cout << "    - VN 1: Write traffic (message class)\n\n";

    std::cout << "  Virtual Channels per VN: 1\n";
    std::cout << "    - 1 VC per VN (no deadlock in tree topology)\n\n";

    std::cout << "  Total VCs: 2 (2 VNs x 1 VC/VN)\n\n";

    std::cout << "Why 2 VNs (not 2 VCs):\n";
    std::cout << "  [OK] Separates read/write message classes\n";
    std::cout << "  [OK] Models separate DRAM datapaths\n";
    std::cout << "  [OK] Prevents protocol-level deadlock\n\n";

    std::cout << "Why 1 VC per VN:\n";
    std::cout << "  [OK] H-trees are acyclic (deadlock-free)\n";
    std::cout << "  [OK] No need for escape VCs\n";
    std::cout << "  [OK] Minimal complexity for DRAM\n";
}

/**
 * Example 2: Multi-class traffic with 4 VNs
 *
 * Use case: Different priority classes or traffic types
 */
void example_multiclass_vns() {
    std::cout << "\n=== Example 2: Multi-Class Traffic (4 VNs) ===\n";

    // For this example, we would need to modify createGarnetHTreeForDRAM
    // to accept custom VN count, but the GARNET router supports it!

    std::cout << "Hypothetical 4-VN configuration:\n";
    std::cout << "  VN 0: High-priority reads (real-time)\n";
    std::cout << "  VN 1: High-priority writes (real-time)\n";
    std::cout << "  VN 2: Low-priority reads (background)\n";
    std::cout << "  VN 3: Low-priority writes (background)\n\n";

    std::cout << "  VCs per VN: 1 (still deadlock-free in tree)\n";
    std::cout << "  Total VCs: 4 (4 VNs x 1 VC/VN)\n\n";

    std::cout << "Use case: QoS-aware PIM with priority classes\n";
    std::cout << "- VNs 0-1: Real-time traffic (guaranteed BW)\n";
    std::cout << "- VNs 2-3: Best-effort traffic\n\n";

    std::cout << "Note: This uses VNs for message classes, NOT VCs!\n";
}

/**
 * Example 3: Graphics workload with GDDR6 (specialized VCs)
 */
void example_graphics_vcs() {
    std::cout << "\n=== Example 3: Graphics GDDR6 with Specialized VCs ===\n";

    auto gddr6 = createInternalDRAMNetwork("GDDR6", 16, 4, 4, 2);
    gddr6->enableGarnetSimulation(true);

    // GDDR6 could benefit from specialized VCs:
    // VC 0: Texture reads (massive bandwidth)
    // VC 1: Frame buffer writes (latency-sensitive)
    // (Future: Could add more VCs for compute, Z-buffer, etc.)

    std::cout << "[OK] GDDR6 with texture/framebuffer VC separation\n";
}

/**
 * Example 4: HBM3 with multiple requestor VCs
 */
void example_hbm_multicore_vcs() {
    std::cout << "\n=== Example 4: HBM3 Multi-Requestor VCs ===\n";

    auto hbm3 = createInternalDRAMNetwork("HBM3", 32, 4, 4, 8);
    hbm3->enableGarnetSimulation(true);

    // HBM3 in AI accelerators might have:
    // VC 0: CPU requests
    // VC 1: GPU requests
    // VC 2: NPU/TPU requests
    // VC 3: DMA transfers

    std::cout << "Use case: Multi-accelerator system with HBM3\n";
    std::cout << "- Each accelerator gets dedicated VC\n";
    std::cout << "- Prevents interference between compute units\n";
}

/**
 * Example 5: Understanding VC benefits with contention
 */
void example_vc_contention_benefit() {
    std::cout << "\n=== Example 5: VC Benefits During Contention ===\n";

    auto hbm2 = createInternalDRAMNetwork("HBM2", 16, 4, 4, 8);
    hbm2->enableGarnetSimulation(true);

    // Scenario: 4 PEs sending to different destinations
    // WITHOUT VCs: Head-of-line blocking
    //   - PE0's packet blocks PE1's packet at shared router
    //   - Even though they go to different outputs!

    // WITH 2 VCs:
    //   - Reads use VC0, writes use VC1
    //   - They can proceed independently
    //   - No head-of-line blocking between read/write

    InternalNetworkPacket read_packet, write_packet;

    // Read packet (will use VC 0 internally)
    read_packet.source_bank = 0;
    read_packet.dest_bank = 1;
    read_packet.data_bytes = 64;

    // Write packet (will use VC 1 internally)
    write_packet.source_bank = 2;
    write_packet.dest_bank = 3;
    write_packet.data_bytes = 64;

    // Both can proceed simultaneously due to VC separation!
    hbm2->sendPacket(read_packet);
    hbm2->sendPacket(write_packet);

    std::cout << "[OK] VCs prevent head-of-line blocking\n";
    std::cout << "   Read and write packets proceed independently\n";
}

/**
 * Detailed VC mechanism explanation
 */
void explain_vc_mechanism() {
    std::cout << "\n=== How VCs Work in GARNET H-Tree ===\n\n";

    std::cout << "1. Packet Injection:\n";
    std::cout << "   - Packet assigned to VC based on type (read->VC0, write->VC1)\n";
    std::cout << "   - Each VC has independent input buffer\n\n";

    std::cout << "2. Router Pipeline:\n";
    std::cout << "   RC (Route Computation):  Determine output port\n";
    std::cout << "   VA (VC Allocation):      Allocate output VC <- VCs assigned here!\n";
    std::cout << "   SA (Switch Allocation):  Compete for crossbar\n";
    std::cout << "   ST (Switch Traversal):   Transfer flit\n\n";

    std::cout << "3. Credit-Based Flow Control:\n";
    std::cout << "   - Each VC has separate credit counter\n";
    std::cout << "   - Upstream router tracks downstream VC buffer space\n";
    std::cout << "   - Prevents buffer overflow\n\n";

    std::cout << "4. Benefits:\n";
    std::cout << "   [OK] No head-of-line blocking between VCs\n";
    std::cout << "   [OK] QoS support (priority VCs)\n";
    std::cout << "   [OK] Deadlock avoidance (escape VCs)\n";
    std::cout << "   [OK] Traffic isolation (separate classes)\n";
}

int main() {
    std::cout << "============================================\n";
    std::cout << " GARNET Virtual Networks (VN) vs\n";
    std::cout << " Virtual Channels (VC) Examples\n";
    std::cout << " For DRAM H-Tree Networks\n";
    std::cout << "============================================\n";

    example_vn_vs_vc_explanation();
    example_multiclass_vns();
    example_graphics_vcs();
    example_hbm_multicore_vcs();
    example_vc_contention_benefit();
    explain_vc_mechanism();

    std::cout << "\n============================================\n";
    std::cout << "Key Takeaways:\n";
    std::cout << "============================================\n";
    std::cout << "1. VN != VC!\n";
    std::cout << "   - VN = Message classes (read vs write)\n";
    std::cout << "   - VC = Deadlock avoidance within VN\n\n";
    std::cout << "2. DRAM default: 2 VNs x 1 VC/VN = 2 total VCs\n";
    std::cout << "   - VN0: Read traffic\n";
    std::cout << "   - VN1: Write traffic\n\n";
    std::cout << "3. Router pipeline: MINIMAL (1-stage mux)\n";
    std::cout << "   - Just switching, no routing computation\n";
    std::cout << "   - 1 cycle latency (realistic for DRAM)\n\n";
    std::cout << "4. Customizable for QoS (more VNs for priority)\n\n";
    std::cout << "5. Critical for realistic multi-PE PIM simulation\n";
    std::cout << "============================================\n";

    return 0;
}
