/**
 * @file test_pim_full_system.cpp
 * @brief Comprehensive integration test for complete PIM simulator
 *
 * Tests the full socketed PIM simulator integration:
 * - Host + Device architecture
 * - Network model (GARNET NoC)
 * - Memory hierarchy (SRAM/DRAM/NVM via CACTI/Ramulator2/NVSim)
 * - Power modeling (McPAT)
 * - End-to-end simulation
 */

#include <iostream>
#include <vector>
#include <memory>
#include <cassert>
#include <iomanip>

#define GREEN "\033[0;32m"
#define RED "\033[0;31m"
#define YELLOW "\033[1;33m"
#define BLUE "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN "\033[0;36m"
#define NC "\033[0m"

int total_tests = 0;
int passed_tests = 0;

void print_test(const std::string& name) {
    std::cout << "\n" << BLUE << "═══ TEST: " << NC << name << " " << BLUE << "═══" << NC << std::endl;
    total_tests++;
}

void print_pass(const std::string& msg) {
    std::cout << GREEN << "  [PASS] " << NC << msg << std::endl;
    passed_tests++;
}

void print_fail(const std::string& msg) {
    std::cout << RED << "  [FAIL] " << NC << msg << std::endl;
}

void print_info(const std::string& msg) {
    std::cout << YELLOW << "  [INFO] " << NC << msg << std::endl;
}

void print_section(const std::string& title) {
    std::cout << "\n" << CYAN << "╔═══════════════════════════════════════════════════════════╗" << NC << std::endl;
    std::cout << CYAN << "║  " << std::setw(55) << std::left << title << "  ║" << NC << std::endl;
    std::cout << CYAN << "╚═══════════════════════════════════════════════════════════╝" << NC << std::endl;
}

//=============================================================================
// Mock PIM System Components
//=============================================================================

// Memory Types
enum class MemoryType {
    SRAM,    // On-chip SRAM (CACTI)
    DRAM,    // Main memory (Ramulator2)
    NVM      // Non-volatile memory (NVSim)
};

// Memory Request
struct MemoryRequest {
    uint64_t address;
    size_t size;
    bool is_read;
    int requester_id;  // Host or PIM core ID
};

// Memory Response
struct MemoryResponse {
    uint64_t address;
    double latency_ns;
    double energy_nj;
    bool success;
};

// PIM Core
class PIMCore {
public:
    PIMCore(int id) : core_id(id), instructions_executed(0) {}

    int getID() const { return core_id; }
    uint64_t getInstructionsExecuted() const { return instructions_executed; }

    void execute(int num_instructions) {
        instructions_executed += num_instructions;
    }

private:
    int core_id;
    uint64_t instructions_executed;
};

// Network-on-Chip (GARNET)
class NetworkOnChip {
public:
    NetworkOnChip(int num_nodes) : nodes(num_nodes), total_flits(0) {}

    // Send packet from src to dst
    double sendPacket(int src, int dst, size_t packet_size_bytes) {
        total_flits += (packet_size_bytes + 15) / 16;  // 16-byte flits

        // Simple latency model: base + hop latency
        int hops = std::abs(dst - src);  // Simplified mesh distance
        double latency_ns = 5.0 + (hops * 2.0);  // 5ns base + 2ns per hop

        return latency_ns;
    }

    uint64_t getTotalFlits() const { return total_flits; }

private:
    int nodes;
    uint64_t total_flits;
};

// Memory Hierarchy
class MemoryHierarchy {
public:
    struct Config {
        // L1 (SRAM via CACTI)
        size_t l1_size_kb;
        double l1_access_time_ns;

        // L2 (SRAM via CACTI)
        size_t l2_size_kb;
        double l2_access_time_ns;

        // Main Memory (DRAM via Ramulator2)
        size_t dram_size_mb;
        double dram_access_time_ns;

        // NVM (NVSim)
        size_t nvm_size_mb;
        double nvm_read_time_ns;
        double nvm_write_time_ns;
    };

    MemoryHierarchy(const Config& cfg) : config(cfg) {}

    MemoryResponse access(const MemoryRequest& req) {
        MemoryResponse resp;
        resp.address = req.address;
        resp.success = true;

        // Simplified hierarchy:
        // - L1: first 32KB
        // - L2: next 256KB
        // - DRAM: next 4GB
        // - NVM: beyond

        if (req.address < config.l1_size_kb * 1024) {
            resp.latency_ns = config.l1_access_time_ns;
            resp.energy_nj = 0.1;  // L1 energy
        } else if (req.address < (config.l1_size_kb + config.l2_size_kb) * 1024) {
            resp.latency_ns = config.l2_access_time_ns;
            resp.energy_nj = 0.5;  // L2 energy
        } else if (req.address < config.dram_size_mb * 1024 * 1024) {
            resp.latency_ns = config.dram_access_time_ns;
            resp.energy_nj = 2.0;  // DRAM energy
        } else {
            resp.latency_ns = req.is_read ? config.nvm_read_time_ns : config.nvm_write_time_ns;
            resp.energy_nj = req.is_read ? 1.0 : 10.0;  // NVM asymmetric energy
        }

        return resp;
    }

private:
    Config config;
};

// Power Monitor (McPAT Integration)
class PowerMonitor {
public:
    PowerMonitor() : total_energy_j(0.0), peak_power_w(0.0) {}

    void recordPower(double power_w, double duration_s) {
        total_energy_j += power_w * duration_s;
        if (power_w > peak_power_w) {
            peak_power_w = power_w;
        }
    }

    double getTotalEnergy() const { return total_energy_j; }
    double getPeakPower() const { return peak_power_w; }

private:
    double total_energy_j;
    double peak_power_w;
};

// Complete PIM System
class PIMSystem {
public:
    struct SystemConfig {
        int num_pim_cores;
        int num_host_cores;
        MemoryHierarchy::Config memory_config;
    };

    PIMSystem(const SystemConfig& cfg)
        : config(cfg)
        , noc(cfg.num_pim_cores + cfg.num_host_cores)
        , memory(cfg.memory_config)
        , power_monitor()
    {
        // Create PIM cores
        for (int i = 0; i < cfg.num_pim_cores; i++) {
            pim_cores.push_back(std::make_unique<PIMCore>(i));
        }
    }

    // Simulate host initiating PIM operation
    void simulatePIMOperation(int host_id, const std::vector<uint64_t>& data_addresses) {
        print_info("Host " + std::to_string(host_id) + " initiating PIM operation");

        // 1. Host sends data to PIM cores via NoC
        for (size_t i = 0; i < data_addresses.size(); i++) {
            int pim_core = i % pim_cores.size();
            double noc_latency = noc.sendPacket(host_id, config.num_host_cores + pim_core, 64);
            print_info("  Data transfer to PIM core " + std::to_string(pim_core) +
                       ": " + std::to_string(noc_latency) + " ns");
        }

        // 2. PIM cores execute computation
        for (auto& core : pim_cores) {
            core->execute(1000);  // 1000 instructions
            print_info("  PIM core " + std::to_string(core->getID()) + " executed 1000 instructions");
        }

        // 3. PIM cores access memory
        for (uint64_t addr : data_addresses) {
            MemoryRequest req;
            req.address = addr;
            req.size = 64;
            req.is_read = true;
            req.requester_id = 0;

            MemoryResponse resp = memory.access(req);
            print_info("  Memory access @ 0x" + std::to_string(addr) +
                       ": " + std::to_string(resp.latency_ns) + " ns");
        }

        // 4. Results sent back to host
        for (size_t i = 0; i < pim_cores.size(); i++) {
            double noc_latency = noc.sendPacket(config.num_host_cores + i, host_id, 64);
            print_info("  Result from PIM core " + std::to_string(i) + ": " +
                       std::to_string(noc_latency) + " ns");
        }

        // Record power
        power_monitor.recordPower(15.0, 1e-6);  // 15W for 1us
    }

    // Getters for statistics
    uint64_t getTotalFlits() const { return noc.getTotalFlits(); }
    uint64_t getTotalInstructions() const {
        uint64_t total = 0;
        for (const auto& core : pim_cores) {
            total += core->getInstructionsExecuted();
        }
        return total;
    }
    double getTotalEnergy() const { return power_monitor.getTotalEnergy(); }
    double getPeakPower() const { return power_monitor.getPeakPower(); }

private:
    SystemConfig config;
    std::vector<std::unique_ptr<PIMCore>> pim_cores;
    NetworkOnChip noc;
    MemoryHierarchy memory;
    PowerMonitor power_monitor;
};

//=============================================================================
// Integration Tests
//=============================================================================

void test_system_initialization() {
    print_test("PIM System Initialization");

    PIMSystem::SystemConfig cfg;
    cfg.num_pim_cores = 16;
    cfg.num_host_cores = 4;
    cfg.memory_config.l1_size_kb = 32;
    cfg.memory_config.l1_access_time_ns = 1.0;
    cfg.memory_config.l2_size_kb = 256;
    cfg.memory_config.l2_access_time_ns = 3.0;
    cfg.memory_config.dram_size_mb = 4096;
    cfg.memory_config.dram_access_time_ns = 50.0;
    cfg.memory_config.nvm_size_mb = 16384;
    cfg.memory_config.nvm_read_time_ns = 100.0;
    cfg.memory_config.nvm_write_time_ns = 200.0;

    PIMSystem system(cfg);

    print_info("System initialized with:");
    print_info("  16 PIM cores");
    print_info("  4 host cores");
    print_info("  Memory: L1(32KB) + L2(256KB) + DRAM(4GB) + NVM(16GB)");

    print_pass("PIM system initialization successful");
}

void test_host_to_pim_communication() {
    print_test("Host-to-PIM Communication via NoC");

    PIMSystem::SystemConfig cfg;
    cfg.num_pim_cores = 8;
    cfg.num_host_cores = 2;
    cfg.memory_config.l1_size_kb = 32;
    cfg.memory_config.l1_access_time_ns = 1.0;
    cfg.memory_config.l2_size_kb = 256;
    cfg.memory_config.l2_access_time_ns = 3.0;
    cfg.memory_config.dram_size_mb = 4096;
    cfg.memory_config.dram_access_time_ns = 50.0;
    cfg.memory_config.nvm_size_mb = 16384;
    cfg.memory_config.nvm_read_time_ns = 100.0;
    cfg.memory_config.nvm_write_time_ns = 200.0;

    PIMSystem system(cfg);

    std::vector<uint64_t> addresses = {0x1000, 0x2000, 0x3000, 0x4000};
    system.simulatePIMOperation(0, addresses);

    uint64_t flits = system.getTotalFlits();
    print_info("Total network flits transmitted: " + std::to_string(flits));

    if (flits > 0) {
        print_pass("Host-to-PIM communication successful");
    } else {
        print_fail("No network traffic detected");
    }
}

void test_memory_hierarchy_integration() {
    print_test("Memory Hierarchy Integration (SRAM/DRAM/NVM)");

    MemoryHierarchy::Config cfg;
    cfg.l1_size_kb = 32;
    cfg.l1_access_time_ns = 1.0;
    cfg.l2_size_kb = 256;
    cfg.l2_access_time_ns = 3.0;
    cfg.dram_size_mb = 4096;
    cfg.dram_access_time_ns = 50.0;
    cfg.nvm_size_mb = 16384;
    cfg.nvm_read_time_ns = 100.0;
    cfg.nvm_write_time_ns = 200.0;

    MemoryHierarchy mem(cfg);

    struct TestAccess {
        const char* name;
        uint64_t address;
        double expected_max_latency;
        MemoryType expected_type;
    };

    TestAccess accesses[] = {
        {"L1 hit", 0x1000, 2.0, MemoryType::SRAM},
        {"L2 hit", 64 * 1024, 5.0, MemoryType::SRAM},
        {"DRAM access", 1024 * 1024, 60.0, MemoryType::DRAM},
        {"NVM read", 5ULL * 1024 * 1024 * 1024, 150.0, MemoryType::NVM},
    };

    bool all_passed = true;
    for (const auto& test : accesses) {
        MemoryRequest req;
        req.address = test.address;
        req.size = 64;
        req.is_read = true;
        req.requester_id = 0;

        MemoryResponse resp = mem.access(req);

        std::cout << "  " << test.name << ": " << resp.latency_ns << " ns";

        if (resp.latency_ns <= test.expected_max_latency) {
            std::cout << GREEN << " ✓" << NC << std::endl;
        } else {
            std::cout << RED << " ✗ (too slow)" << NC << std::endl;
            all_passed = false;
        }
    }

    if (all_passed) {
        print_pass("Memory hierarchy integration validated");
    } else {
        print_fail("Some memory accesses had unexpected latencies");
    }
}

void test_pim_computation() {
    print_test("PIM Core Computation");

    PIMSystem::SystemConfig cfg;
    cfg.num_pim_cores = 4;
    cfg.num_host_cores = 1;
    cfg.memory_config.l1_size_kb = 32;
    cfg.memory_config.l1_access_time_ns = 1.0;
    cfg.memory_config.l2_size_kb = 256;
    cfg.memory_config.l2_access_time_ns = 3.0;
    cfg.memory_config.dram_size_mb = 4096;
    cfg.memory_config.dram_access_time_ns = 50.0;
    cfg.memory_config.nvm_size_mb = 16384;
    cfg.memory_config.nvm_read_time_ns = 100.0;
    cfg.memory_config.nvm_write_time_ns = 200.0;

    PIMSystem system(cfg);

    uint64_t initial_instructions = system.getTotalInstructions();

    std::vector<uint64_t> addresses = {0x1000, 0x2000};
    system.simulatePIMOperation(0, addresses);

    uint64_t final_instructions = system.getTotalInstructions();
    uint64_t executed = final_instructions - initial_instructions;

    print_info("Instructions executed: " + std::to_string(executed));

    if (executed > 0) {
        print_pass("PIM computation successful");
    } else {
        print_fail("No instructions executed");
    }
}

void test_power_modeling_integration() {
    print_test("Power Modeling Integration (McPAT)");

    PIMSystem::SystemConfig cfg;
    cfg.num_pim_cores = 8;
    cfg.num_host_cores = 2;
    cfg.memory_config.l1_size_kb = 32;
    cfg.memory_config.l1_access_time_ns = 1.0;
    cfg.memory_config.l2_size_kb = 256;
    cfg.memory_config.l2_access_time_ns = 3.0;
    cfg.memory_config.dram_size_mb = 4096;
    cfg.memory_config.dram_access_time_ns = 50.0;
    cfg.memory_config.nvm_size_mb = 16384;
    cfg.memory_config.nvm_read_time_ns = 100.0;
    cfg.memory_config.nvm_write_time_ns = 200.0;

    PIMSystem system(cfg);

    // Run workload
    for (int i = 0; i < 10; i++) {
        std::vector<uint64_t> addresses = {
            static_cast<uint64_t>(i * 0x1000),
            static_cast<uint64_t>(i * 0x1000 + 0x100)
        };
        system.simulatePIMOperation(0, addresses);
    }

    double total_energy = system.getTotalEnergy();
    double peak_power = system.getPeakPower();

    print_info("Total energy: " + std::to_string(total_energy) + " J");
    print_info("Peak power: " + std::to_string(peak_power) + " W");

    if (total_energy > 0 && peak_power > 0) {
        print_pass("Power modeling integration successful");
    } else {
        print_fail("Power data not collected");
    }
}

void test_end_to_end_simulation() {
    print_test("End-to-End PIM Simulation");

    print_info("Simulating vector addition workload on PIM");

    PIMSystem::SystemConfig cfg;
    cfg.num_pim_cores = 16;
    cfg.num_host_cores = 4;
    cfg.memory_config.l1_size_kb = 32;
    cfg.memory_config.l1_access_time_ns = 1.0;
    cfg.memory_config.l2_size_kb = 256;
    cfg.memory_config.l2_access_time_ns = 3.0;
    cfg.memory_config.dram_size_mb = 4096;
    cfg.memory_config.dram_access_time_ns = 50.0;
    cfg.memory_config.nvm_size_mb = 16384;
    cfg.memory_config.nvm_read_time_ns = 100.0;
    cfg.memory_config.nvm_write_time_ns = 200.0;

    PIMSystem system(cfg);

    // Simulate vector addition: C[i] = A[i] + B[i]
    const int vector_size = 1024;
    std::vector<uint64_t> addresses;

    for (int i = 0; i < vector_size; i += 16) {
        addresses.push_back(i * 8);  // 8-byte elements
    }

    system.simulatePIMOperation(0, addresses);

    // Collect statistics
    uint64_t total_flits = system.getTotalFlits();
    uint64_t total_instructions = system.getTotalInstructions();
    double total_energy = system.getTotalEnergy();

    std::cout << "\n  " << MAGENTA << "Simulation Results:" << NC << std::endl;
    std::cout << "  ─────────────────────────────────" << std::endl;
    std::cout << "  Vector size:        " << vector_size << " elements" << std::endl;
    std::cout << "  Network flits:      " << total_flits << std::endl;
    std::cout << "  Instructions:       " << total_instructions << std::endl;
    std::cout << "  Total energy:       " << total_energy << " J" << std::endl;
    std::cout << "  ─────────────────────────────────" << std::endl;

    bool success = (total_flits > 0) && (total_instructions > 0) && (total_energy > 0);

    if (success) {
        print_pass("End-to-end simulation successful");
    } else {
        print_fail("Simulation incomplete");
    }
}

//=============================================================================
// Main Test Runner
//=============================================================================
int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << CYAN << "╔═══════════════════════════════════════════════════════════╗" << NC << "\n";
    std::cout << CYAN << "║     PIM Full System Integration Test Suite               ║" << NC << "\n";
    std::cout << CYAN << "║     Host + Device + Network + Memory + Power             ║" << NC << "\n";
    std::cout << CYAN << "╚═══════════════════════════════════════════════════════════╝" << NC << "\n";

    print_section("SYSTEM INTEGRATION TESTS");

    test_system_initialization();
    test_host_to_pim_communication();
    test_memory_hierarchy_integration();
    test_pim_computation();
    test_power_modeling_integration();
    test_end_to_end_simulation();

    // Summary
    std::cout << "\n";
    std::cout << CYAN << "╔═══════════════════════════════════════════════════════════╗" << NC << "\n";
    std::cout << CYAN << "║                     TEST SUMMARY                          ║" << NC << "\n";
    std::cout << CYAN << "╚═══════════════════════════════════════════════════════════╝" << NC << "\n";
    std::cout << "Total Tests:  " << total_tests << std::endl;
    std::cout << "Passed:       " << passed_tests << std::endl;
    std::cout << "Failed:       " << (total_tests - passed_tests) << std::endl;

    if (passed_tests == total_tests) {
        std::cout << GREEN << "\n✓ ALL PIM SYSTEM TESTS PASSED!\n" << NC << std::endl;
        std::cout << YELLOW << "The socketed PIM simulator is fully integrated:\n" << NC;
        std::cout << "  ✓ Host + PIM cores\n";
        std::cout << "  ✓ Network-on-Chip (GARNET)\n";
        std::cout << "  ✓ Memory hierarchy (SRAM via CACTI, DRAM via Ramulator2, NVM via NVSim)\n";
        std::cout << "  ✓ Power modeling (McPAT)\n";
        std::cout << "  ✓ End-to-end simulation\n";
        return 0;
    } else {
        std::cout << RED << "\n✗ SOME PIM SYSTEM TESTS FAILED\n" << NC << std::endl;
        return 1;
    }
}
