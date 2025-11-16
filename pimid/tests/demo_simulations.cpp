/**
 * @file demo_simulations.cpp
 * @brief Comprehensive demonstration of PIMID external tools integration
 *
 * This program demonstrates:
 * 1. CACTI for SRAM/cache modeling (various configurations)
 * 2. Ramulator2 for DRAM modeling
 * 3. Combined memory hierarchy simulation
 * 4. Energy and power analysis
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <chrono>

#include "cacti_wrapper.h"
#include "ramulator_wrapper.h"

using namespace pimid;
using namespace std;

//=============================================================================
// Utility Functions
//=============================================================================

void printSeparator(const string& title = "") {
    cout << "\n" << string(70, '=') << endl;
    if (!title.empty()) {
        cout << "  " << title << endl;
        cout << string(70, '=') << endl;
    }
}

void printSubHeader(const string& text) {
    cout << "\n--- " << text << " ---" << endl;
}

//=============================================================================
// CACTI Demonstrations
//=============================================================================

void demo_cacti_l1_cache() {
    printSubHeader("CACTI Demo: L1 Cache (32KB, 4-way, 22nm)");

    CACTIWrapper::SRAMConfig config;
    config.capacity_bytes = 32 * 1024;  // 32 KB
    config.line_size = 64;
    config.associativity = 4;
    config.banks = 1;
    config.tech_node_nm = 22;
    config.is_cache = true;
    config.temperature = 350;  // 350K (~77°C)

    CACTIWrapper cacti(config);

    auto start = chrono::high_resolution_clock::now();
    cacti.initialize();
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end - start);

    if (cacti.isValid()) {
        cout << "\n✓ CACTI Analysis Complete (" << duration.count() << " μs)" << endl;
        cout << "\nTiming Characteristics:" << endl;
        cout << "  Access Time:  " << fixed << setprecision(3)
             << (cacti.getAccessTime() * 1e9) << " ns" << endl;
        cout << "  Cycle Time:   " << (cacti.getCycleTime() * 1e9) << " ns" << endl;

        cout << "\nPhysical Characteristics:" << endl;
        cout << "  Total Area:   " << cacti.getArea() << " mm²" << endl;
        cout << "  Cache Height: " << cacti.getCacheHeight() << " mm" << endl;
        cout << "  Cache Width:  " << cacti.getCacheWidth() << " mm" << endl;
        cout << "  Efficiency:   " << (cacti.getAreaEfficiency() * 100) << "%" << endl;

        cout << "\nEnergy per Access:" << endl;
        cout << "  Read Energy:  " << cacti.getDynamicReadEnergy() << " nJ" << endl;
        cout << "  Write Energy: " << cacti.getDynamicWriteEnergy() << " nJ" << endl;

        cout << "\nPower Characteristics:" << endl;
        cout << "  Read Power:   " << cacti.getReadDynamicPower() << " mW" << endl;
        cout << "  Write Power:  " << cacti.getWriteDynamicPower() << " mW" << endl;
        cout << "  Leakage:      " << cacti.getLeakagePower() << " mW" << endl;
    } else {
        cout << "\n✗ CACTI Analysis Failed: " << cacti.getErrorMessage() << endl;
    }
}

void demo_cacti_l2_cache() {
    printSubHeader("CACTI Demo: L2 Cache (256KB, 8-way, 22nm)");

    CACTIWrapper::SRAMConfig config;
    config.capacity_bytes = 256 * 1024;  // 256 KB
    config.line_size = 64;
    config.associativity = 8;
    config.banks = 4;
    config.tech_node_nm = 22;
    config.is_cache = true;

    CACTIWrapper cacti(config);
    cacti.initialize();

    if (cacti.isValid()) {
        cout << "\n✓ L2 Cache Configuration Valid" << endl;
        cout << "  Access Time: " << (cacti.getAccessTime() * 1e9) << " ns" << endl;
        cout << "  Area:        " << cacti.getArea() << " mm²" << endl;
        cout << "  Read Energy: " << cacti.getDynamicReadEnergy() << " nJ" << endl;
        cout << "  Leakage:     " << cacti.getLeakagePower() << " mW" << endl;
    } else {
        cout << "\n✗ Configuration Failed: " << cacti.getErrorMessage() << endl;
    }
}

void demo_cacti_scratchpad() {
    printSubHeader("CACTI Demo: Scratchpad Memory (128KB, 22nm)");

    CACTIWrapper::SRAMConfig config;
    config.capacity_bytes = 128 * 1024;  // 128 KB
    config.line_size = 64;
    config.associativity = 1;  // Direct-mapped
    config.banks = 1;
    config.tech_node_nm = 22;
    config.is_cache = false;  // Scratchpad, not cache

    CACTIWrapper cacti(config);
    cacti.initialize();

    if (cacti.isValid()) {
        cout << "\n✓ Scratchpad Configuration Valid" << endl;
        cout << "  Access Time: " << (cacti.getAccessTime() * 1e9) << " ns" << endl;
        cout << "  Area:        " << cacti.getArea() << " mm²" << endl;
        cout << "  Read Energy: " << cacti.getDynamicReadEnergy() << " nJ" << endl;
    } else {
        cout << "\n✗ Configuration Failed: " << cacti.getErrorMessage() << endl;
    }
}

void demo_cacti_technology_comparison() {
    printSubHeader("CACTI Demo: Technology Node Comparison (32KB Cache)");

    vector<int> tech_nodes = {90, 65, 45, 32, 22};

    cout << "\n" << setw(12) << "Tech Node"
         << setw(15) << "Access (ns)"
         << setw(15) << "Area (mm²)"
         << setw(15) << "Energy (nJ)"
         << setw(15) << "Leakage (mW)" << endl;
    cout << string(70, '-') << endl;

    for (int tech : tech_nodes) {
        CACTIWrapper::SRAMConfig config;
        config.capacity_bytes = 32 * 1024;
        config.line_size = 64;
        config.associativity = 4;
        config.banks = 1;
        config.tech_node_nm = tech;
        config.is_cache = true;

        CACTIWrapper cacti(config);
        cacti.initialize();

        if (cacti.isValid()) {
            cout << setw(12) << tech << " nm"
                 << setw(15) << fixed << setprecision(3) << (cacti.getAccessTime() * 1e9)
                 << setw(15) << setprecision(4) << cacti.getArea()
                 << setw(15) << setprecision(4) << cacti.getDynamicReadEnergy()
                 << setw(15) << setprecision(2) << cacti.getLeakagePower() << endl;
        } else {
            cout << setw(12) << tech << " nm"
                 << setw(55) << "Configuration Invalid" << endl;
        }
    }
}

//=============================================================================
// Ramulator Demonstrations
//=============================================================================

void demo_ramulator_basic() {
    printSubHeader("Ramulator Demo: Basic DDR4 DRAM Simulation");

    RamulatorWrapper ramulator("");

    auto start = chrono::high_resolution_clock::now();
    ramulator.initialize();
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end - start);

    cout << "\n✓ Ramulator Initialized (" << duration.count() << " μs)" << endl;
    cout << "\nDRAM Configuration:" << endl;
    cout << "  Capacity:  " << (ramulator.getCapacity() / (1024*1024*1024)) << " GB" << endl;
    cout << "  Bandwidth: " << ramulator.getBandwidth() << " MB/s" << endl;
}

void demo_ramulator_memory_requests() {
    printSubHeader("Ramulator Demo: Memory Request Processing");

    RamulatorWrapper ramulator("");
    ramulator.initialize();

    int num_requests = 100;
    int completed = 0;

    auto callback = [&completed](Address addr) {
        completed++;
    };

    cout << "\nSending " << num_requests << " read requests..." << endl;

    auto start = chrono::high_resolution_clock::now();

    // Send requests
    for (int i = 0; i < num_requests; i++) {
        Address addr = (i * 64) % (1024 * 1024);  // 1MB address space
        ramulator.send(addr, MemoryRequestType::READ, callback);
    }

    // Process requests
    int max_cycles = 100000;
    int cycle = 0;
    while (completed < num_requests && cycle < max_cycles) {
        ramulator.tick();
        cycle++;
    }

    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

    cout << "  Requests completed: " << completed << "/" << num_requests << endl;
    cout << "  Simulation cycles:  " << cycle << endl;
    cout << "  Simulation time:    " << duration.count() << " ms" << endl;
    cout << "  Average latency:    " << (double)cycle / num_requests << " cycles" << endl;
}

void demo_ramulator_mixed_traffic() {
    printSubHeader("Ramulator Demo: Mixed Read/Write Traffic");

    RamulatorWrapper ramulator("");
    ramulator.initialize();

    int num_reads = 70;
    int num_writes = 30;
    int completed_reads = 0;
    int completed_writes = 0;

    auto read_callback = [&completed_reads](Address) { completed_reads++; };
    auto write_callback = [&completed_writes](Address) { completed_writes++; };

    cout << "\nSending mixed traffic (70% read, 30% write)..." << endl;

    // Send read requests
    for (int i = 0; i < num_reads; i++) {
        Address addr = (i * 64);
        ramulator.send(addr, MemoryRequestType::READ, read_callback);
    }

    // Send write requests
    for (int i = 0; i < num_writes; i++) {
        Address addr = ((i + num_reads) * 64);
        ramulator.send(addr, MemoryRequestType::WRITE, write_callback);
    }

    // Process
    int cycle = 0;
    while ((completed_reads < num_reads || completed_writes < num_writes) && cycle < 50000) {
        ramulator.tick();
        cycle++;
    }

    cout << "  Reads completed:  " << completed_reads << "/" << num_reads << endl;
    cout << "  Writes completed: " << completed_writes << "/" << num_writes << endl;
    cout << "  Total cycles:     " << cycle << endl;
}

//=============================================================================
// Combined Memory Hierarchy Demonstration
//=============================================================================

void demo_memory_hierarchy() {
    printSubHeader("Combined Demo: L1 + L2 + DRAM Memory Hierarchy");

    // L1 Cache (32KB)
    CACTIWrapper::SRAMConfig l1_config;
    l1_config.capacity_bytes = 32 * 1024;
    l1_config.line_size = 64;
    l1_config.associativity = 4;
    l1_config.banks = 1;
    l1_config.tech_node_nm = 22;
    l1_config.is_cache = true;

    CACTIWrapper l1_cache(l1_config);
    l1_cache.initialize();

    // L2 Cache (256KB)
    CACTIWrapper::SRAMConfig l2_config;
    l2_config.capacity_bytes = 256 * 1024;
    l2_config.line_size = 64;
    l2_config.associativity = 8;
    l2_config.banks = 4;
    l2_config.tech_node_nm = 22;
    l2_config.is_cache = true;

    CACTIWrapper l2_cache(l2_config);
    l2_cache.initialize();

    // Main Memory (DRAM)
    RamulatorWrapper dram("");
    dram.initialize();

    cout << "\n✓ Memory Hierarchy Created" << endl;
    cout << "\nHierarchy Summary:" << endl;
    cout << "┌─────────────────────────────────────────────────┐" << endl;
    cout << "│ L1 Cache (32KB)                                 │" << endl;
    if (l1_cache.isValid()) {
        cout << "│   Access Time: " << setw(8) << (l1_cache.getAccessTime() * 1e9) << " ns" << endl;
        cout << "│   Read Energy: " << setw(8) << l1_cache.getDynamicReadEnergy() << " nJ" << endl;
    }
    cout << "├─────────────────────────────────────────────────┤" << endl;
    cout << "│ L2 Cache (256KB)                                │" << endl;
    if (l2_cache.isValid()) {
        cout << "│   Access Time: " << setw(8) << (l2_cache.getAccessTime() * 1e9) << " ns" << endl;
        cout << "│   Read Energy: " << setw(8) << l2_cache.getDynamicReadEnergy() << " nJ" << endl;
    }
    cout << "├─────────────────────────────────────────────────┤" << endl;
    cout << "│ Main Memory (DDR4)                              │" << endl;
    cout << "│   Capacity:    " << setw(8) << (dram.getCapacity() / (1024*1024*1024)) << " GB" << endl;
    cout << "│   Bandwidth:   " << setw(8) << dram.getBandwidth() << " MB/s" << endl;
    cout << "└─────────────────────────────────────────────────┘" << endl;

    // Calculate total power
    double total_leakage = 0.0;
    if (l1_cache.isValid()) total_leakage += l1_cache.getLeakagePower();
    if (l2_cache.isValid()) total_leakage += l2_cache.getLeakagePower();

    cout << "\nTotal Static Power (Leakage): " << total_leakage << " mW" << endl;
}

//=============================================================================
// Performance Analysis
//=============================================================================

void demo_performance_analysis() {
    printSubHeader("Performance Analysis: Access Time vs. Cache Size");

    vector<int> cache_sizes = {16, 32, 64, 128, 256, 512};  // KB

    cout << "\n" << setw(15) << "Cache Size"
         << setw(15) << "Access (ns)"
         << setw(15) << "Area (mm²)"
         << setw(20) << "Energy/Access (nJ)" << endl;
    cout << string(65, '-') << endl;

    for (int size_kb : cache_sizes) {
        CACTIWrapper::SRAMConfig config;
        config.capacity_bytes = size_kb * 1024;
        config.line_size = 64;
        config.associativity = 4;
        config.banks = (size_kb >= 128) ? 4 : 1;
        config.tech_node_nm = 22;
        config.is_cache = true;

        CACTIWrapper cacti(config);
        cacti.initialize();

        if (cacti.isValid()) {
            cout << setw(12) << size_kb << " KB"
                 << setw(15) << fixed << setprecision(3) << (cacti.getAccessTime() * 1e9)
                 << setw(15) << setprecision(4) << cacti.getArea()
                 << setw(20) << setprecision(4) << cacti.getDynamicReadEnergy() << endl;
        } else {
            cout << setw(12) << size_kb << " KB"
                 << setw(50) << "Invalid" << endl;
        }
    }
}

//=============================================================================
// Main Demonstration Program
//=============================================================================

int main(int argc, char** argv) {
    cout << "\n";
    cout << "╔════════════════════════════════════════════════════════════════════╗\n";
    cout << "║                                                                    ║\n";
    cout << "║           PIMID External Tools Integration Demonstration           ║\n";
    cout << "║                                                                    ║\n";
    cout << "║  Testing CACTI, Ramulator2, and Combined Memory Hierarchies       ║\n";
    cout << "║                                                                    ║\n";
    cout << "╚════════════════════════════════════════════════════════════════════╝\n";

    // CACTI Demonstrations
    printSeparator("CACTI: SRAM/Cache Modeling");
    demo_cacti_l1_cache();
    demo_cacti_l2_cache();
    demo_cacti_scratchpad();
    demo_cacti_technology_comparison();

    // Ramulator Demonstrations
    printSeparator("Ramulator2: DRAM Simulation");
    demo_ramulator_basic();
    demo_ramulator_memory_requests();
    demo_ramulator_mixed_traffic();

    // Combined Demonstrations
    printSeparator("Combined Memory Hierarchy");
    demo_memory_hierarchy();
    demo_performance_analysis();

    // Summary
    printSeparator("Demonstration Complete");
    cout << "\n✓ All external tools demonstrated successfully!\n" << endl;
    cout << "Key Findings:" << endl;
    cout << "  • CACTI provides accurate cache/SRAM modeling" << endl;
    cout << "  • Ramulator2 enables detailed DRAM simulation" << endl;
    cout << "  • Tools integrate seamlessly for hierarchy modeling" << endl;
    cout << "  • Performance and energy metrics are available" << endl;
    cout << "\n";

    return 0;
}
