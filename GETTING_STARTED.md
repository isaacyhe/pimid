# Getting Started with PIMID

This guide will walk you through your first PIMID simulation.

## Prerequisites Check

Before starting, ensure you have:

```bash
# Check C++ compiler
g++ --version  # Should be ≥ 7.0

# Check CMake
cmake --version  # Should be ≥ 3.15

# Check Boost
dpkg -l | grep libboost  # Ubuntu/Debian
rpm -qa | grep boost     # RHEL/CentOS
```

## Installation Steps

### 1. Clone and Build

```bash
# Clone the repository
cd /home/user/pimid-dev/pimid

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 2. Verify Installation

```bash
# Check binaries
ls -la pimid_*

# Expected output:
# pimid_host
# pimid_device
# pimid_standalone
```

## Your First Simulation

### Step 1: Understand the Configuration

PIMID uses a hierarchical YAML configuration:

```
pimid_config.yaml          # Main config
├── host_config.yaml       # Host processor setup
├── device_config.yaml     # PIM device setup
├── dram_config.yaml       # Memory configuration
├── network_config.yaml    # NoC configuration
└── power_config.yaml      # Power modeling
```

### Step 2: Use Example Configuration

```bash
# Copy example configuration
cp -r ../configs/examples/ ./my_first_sim/
cd my_first_sim
```

### Step 3: Prepare a Simple Workload

Create a simple test program:

```cpp
// test_workload.cpp
#include <iostream>
#include <vector>

#include "pimid/pimid_hooks.h"

int main() {
    const size_t N = 1024;
    std::vector<int> data(N);

    // Initialize data
    for (size_t i = 0; i < N; i++) {
        data[i] = i;
    }

    // Mark region of interest for simulation
    pimid_roi_begin();

    // Offload to PIM
    pimid_offload_begin();

    // Simple computation
    for (size_t i = 0; i < N; i++) {
        data[i] = data[i] * 2 + 1;
    }

    pimid_offload_end();

    pimid_roi_end();

    std::cout << "Computation complete!" << std::endl;
    return 0;
}
```

Compile it:

```bash
g++ -o test_workload test_workload.cpp -I../include
```

### Step 4: Run Your First Simulation

#### Option A: Standalone Mode (Easiest for Testing)

```bash
./pimid_standalone \
    --config=pimid_config.yaml \
    --workload=./test_workload \
    --output=results/
```

#### Option B: Co-Simulation Mode (Full Features)

Terminal 1 (Device):
```bash
./pimid_device pimid_config.yaml
# Wait for: "Device engine initialized, waiting for host..."
```

Terminal 2 (Host):
```bash
./pimid_host pimid_config.yaml ./test_workload
```

### Step 5: View Results

```bash
# View statistics
cat results/stats.txt

# Key metrics to look for:
# - Total cycles
# - Memory accesses
# - Network packets (if enabled)
# - Energy consumption (if power modeling enabled)
```

Example output:
```
======= PIMID Simulation Statistics =======
Simulation Mode: co-simulation
Memory Technology: DRAM

Host Engine:
  Total Cycles: 1,250,000
  Instructions: 10,000
  L1 Cache Hits: 8,500
  L1 Cache Misses: 1,500
  Offload Requests: 1

Device Engine:
  Total Cycles: 500,000
  PEs Active: 4
  Memory Accesses: 2,048
  Average PE Utilization: 75%

Memory System:
  DRAM Reads: 1,024
  DRAM Writes: 1,024
  Row Buffer Hits: 856 (83.6%)
  Average Access Latency: 85 cycles

Network (GARNET):
  Total Packets: 512
  Average Packet Latency: 12 cycles
  Network Utilization: 15%

Power (McPAT):
  Host Power: 25.4W
  PE Power: 8.2W
  Memory Power: 5.1W
  Network Power: 2.3W
  Total Energy: 204.5 mJ
==========================================
```

## Common Configurations

### Configuration 1: Compare Memory Technologies

Create three configs with different memory technologies:

```yaml
# dram_comparison.yaml
memory:
  technology: "DRAM"
```

```yaml
# sram_comparison.yaml
memory:
  technology: "SRAM"
```

```yaml
# mram_comparison.yaml
memory:
  technology: "STT_MRAM"
```

Run and compare:
```bash
./pimid_standalone --config=dram_comparison.yaml ./test_workload > dram_results.txt
./pimid_standalone --config=sram_comparison.yaml ./test_workload > sram_results.txt
./pimid_standalone --config=mram_comparison.yaml ./test_workload > mram_results.txt
```

### Configuration 2: PE Placement Study

Modify device_config.yaml:

```yaml
# Rank-level PEs
pe_placement:
  level: "RANK"
  num_pes_per_level: 4

# vs Bank-level PEs
pe_placement:
  level: "BANK"
  num_pes_per_level: 16
```

### Configuration 3: Network Topology Exploration

Modify network_config.yaml:

```yaml
# 2D Mesh
topology: "MESH_2D"
num_rows: 4
num_cols: 4

# vs Crossbar
topology: "CROSSBAR"
```

## Debugging Tips

### Enable Debug Mode

```yaml
# pimid_config.yaml
output:
  log_level: "DEBUG"
```

### Check Communication

```bash
# Test socket communication
netstat -an | grep 50000

# Should show:
# tcp  0  0  127.0.0.1:50000  0.0.0.0:*  LISTEN
```

### Verbose Output

```bash
./pimid_standalone --config=pimid_config.yaml --verbose ./test_workload
```

### Validate Configuration

```bash
./pimid_standalone --validate-config pimid_config.yaml
```

## Next Steps

1. **Read the full documentation**: See `docs/` directory
2. **Explore examples**: Check `tests/benchmarks/` for more complex workloads
3. **Customize configurations**: Modify YAML files to match your architecture
4. **Extend PIMID**: Add custom memory models, schedulers, or network topologies

## Troubleshooting

### Problem: "Cannot connect to device"

**Solution**: Ensure device simulator starts first and check port availability
```bash
# Check if port is already in use
lsof -i :50000

# Try different port
communication:
  port: 50001
```

### Problem: "Memory model initialization failed"

**Solution**: Verify external tool configurations
```bash
# Check Ramulator config
cat configs/memory/ramulator_ddr4.cfg

# Check paths in memory_config.yaml
ls -la configs/memory/
```

### Problem: "Build errors"

**Solution**: Check dependencies
```bash
# Install missing dependencies (Ubuntu)
sudo apt-get install libboost-all-dev libyaml-cpp-dev

# Clean and rebuild
rm -rf build && mkdir build && cd build && cmake .. && make
```

## Getting Help

- **Documentation**: `docs/`
- **Examples**: `tests/benchmarks/`
- **Issues**: GitHub Issues
- **Email**: isaacyhe@acm.org

Happy simulating with PIMID!
