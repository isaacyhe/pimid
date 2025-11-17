# Subarray-Level PIM Access Implementation Status

## Quick Answer

**Partially Implemented** - Subarray-level PIM is supported through the **PIM controller layer**, but Ramulator2 itself only models DRAM at the **bank level**.

## What IS Implemented ✅

### 1. **PIM Controller Layer** (Full Subarray Support)

The PIM integration adds subarray-level modeling **on top of** Ramulator:

#### PIMRequestPayload - Subarray Granularity
```cpp
// pimid/memory_models/include/pim_request_payload.h
enum class PIMGranularity {
    SUBARRAY,          // ✅ DEFINED - PIM at subarray level
    BANK,              // ✅ DEFINED
    BANK_GROUP,        // ✅ DEFINED
    CHIP,              // ✅ DEFINED
    RANK,              // ✅ DEFINED
    MEMORY_CONTROLLER, // ✅ DEFINED
};

struct PIMRequestPayload {
    int target_subarray;       // ✅ Subarray ID tracked
    int source_subarray;       // ✅ For network transfers
    int dest_subarray;         // ✅ For network transfers
    // ...
};
```

#### PIMBandwidthTracker - Subarray Bandwidth Limits
```cpp
// pimid/memory_models/src/pim_bandwidth_tracker.cpp
void PIMBandwidthTracker::initializeBandwidthLimits() {
    // ✅ Subarray: GSA datapath (256 bits for DDR4, 512 for HBM2)
    subarray_port_bits_ = dram_arch_->datapath.gsa_datapath_bits.value_bits;

    // ✅ Calculate subarray bandwidth limit
    subarray_bw_limit_ = calculateBandwidth(subarray_port_bits_, clock_freq_GHz_);

    // DDR4: 256 bits @ 1.2 GHz = 38.4 GB/s (internal GSA bandwidth)
}

double PIMBandwidthTracker::getBandwidthLimit(PIMGranularity granularity) const {
    switch (granularity) {
        case PIMGranularity::SUBARRAY:  // ✅ IMPLEMENTED
            return subarray_bw_limit_;   // Returns 38.4 GB/s for DDR4
        // ...
    }
}
```

#### InternalDRAMNetwork - Subarray-to-Subarray Transfers
```cpp
// pimid/memory_models/src/internal_dram_network.cpp
enum class NetworkLevel {
    SUBARRAY_NETWORK,      // ✅ Within-bank network (subarray-to-subarray)
    BANK_NETWORK,          // ✅ Within-bank-group network
    BANK_GROUP_NETWORK,    // ✅ Within-chip network
    CHIP_NETWORK,          // ✅ Within-rank network
};

void InternalDRAMNetwork::configureDDR4Network() {
    // ✅ Subarray network (within bank): Uses GSA (256 bits) but serialized
    subarray_network_config_.link_width_bits = 64;  // Prefetch width
    subarray_network_config_.frequency_GHz = 1.2;    // DDR4-2400
    subarray_network_config_.bandwidth_GBs = 9.6;    // 64 bits @ 1.2 GHz
    subarray_network_config_.latency_cycles = 5;     // Short distance
    subarray_network_config_.topology = "crossbar";  // Within bank
}
```

### 2. **Subarray-Level Configuration Files** ✅

Created in recent commit:

#### ZSim Configuration
```cfg
// pimid/external/zsim/tests/pim_pe_subarray_level.cfg
cores = {
    subarray_pes = {
        type = "ALU";      // ✅ ALU cores as subarray PEs
        cores = 256;       // ✅ 1 PE per subarray (16/bank × 16 banks)
        aluLatency = 1;
        simdWidth = 512;   // AVX-512 support
    };
};
```

#### PIMID Configuration
```yaml
# configs/example_pim_config.yaml
pim:
  granularity: SUBARRAY  # ✅ Subarray-level PIM supported
  num_pes: 256           # ✅ 1 PE per subarray
```

## What is NOT Implemented ❌

### 1. **Ramulator2 Core** - No Native Subarray Modeling

Ramulator2 models DRAM at the **bank level**:

```
Ramulator2 Hierarchy:
├── Channel
├── Rank
├── Chip
├── Bank Group
└── Bank          ← LOWEST LEVEL in Ramulator!
    └── Subarray  ← NOT MODELED by Ramulator
```

**Why?**
- JEDEC DRAM specifications define commands at bank level (ACT, PRE, RD, WR)
- Subarrays are internal bank organization not exposed in standard interface
- Subarray details are manufacturer-specific and not standardized

**What Ramulator Models:**
```cpp
// Ramulator commands operate on BANKS, not subarrays
ACT  <bank_id>        // Activate a bank (opens row buffer)
PRE  <bank_id>        // Precharge a bank (closes row buffer)
RD   <bank_id> <col>  // Read from bank's row buffer
WR   <bank_id> <col>  // Write to bank's row buffer
```

**What Ramulator Does NOT Model:**
- Individual subarray activation within a bank
- Subarray-to-subarray data transfers
- Per-subarray row buffers
- Subarray-level timing constraints

### 2. **Address Mapping to Subarrays** - Partial

Ramulator maps addresses to banks but not subarrays:

```cpp
// Ramulator address mapping
Address → Channel → Rank → BankGroup → Bank → Row → Column
                                        ↑
                                    STOPS HERE!

// Missing: Bank → Subarray → LocalRow
```

**Workaround in PIM Controller:**
```cpp
// We estimate subarray from bank and row
int subarray_id = (row_address >> subarray_row_bits) % num_subarrays_per_bank;

// This is an APPROXIMATION, not true Ramulator mapping!
```

### 3. **Per-Subarray State Tracking** - Not in Ramulator

What's missing:
- ❌ Per-subarray row buffer state
- ❌ Subarray-level conflicts (multiple PEs in same subarray)
- ❌ Subarray-local sense amplifier busy/idle state
- ❌ True subarray-to-subarray latency based on distance

## How It Works Together

### Architecture Layers

```
┌──────────────────────────────────────────────────────────┐
│  Application Layer                                       │
│  - ZSim ALU cores configured as subarray PEs             │
│  - 256 PEs (1 per subarray)                             │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│  PIM Controller Layer (PIMID Custom)                     │
│  ✅ Tracks subarray IDs                                  │
│  ✅ Enforces GSA bandwidth limits (38.4 GB/s DDR4)       │
│  ✅ Models subarray-to-subarray network transfers        │
│  ✅ Calculates subarray-level data movement latency      │
└────────────────────┬─────────────────────────────────────┘
                     │
                     ▼
┌──────────────────────────────────────────────────────────┐
│  Ramulator2 (Standard DRAM Simulator)                    │
│  ✅ Bank-level commands (ACT, PRE, RD, WR)               │
│  ✅ DRAM timing constraints (tRCD, tCAS, tRP, tRAS)      │
│  ✅ Row buffer management                                │
│  ❌ Subarray modeling (NOT INCLUDED)                     │
└──────────────────────────────────────────────────────────┘
```

### Example: Subarray PIM Request Flow

```cpp
// 1. Application: PE #5 (in subarray #5) needs data
PIMRequestPayload payload;
payload.granularity = PIMGranularity::SUBARRAY;
payload.pe_id = 5;
payload.target_subarray = 5;
payload.data_bytes = 4096;

// 2. PIM Controller Layer:
//    - Calculates which BANK contains subarray #5
int target_bank = 5 / 16;  // subarray_id / subarrays_per_bank
//    - Enforces GSA bandwidth limit (38.4 GB/s)
uint64_t data_movement_cycles = calculate_data_cycles(4096, 38.4);
//    - If data is in different subarray, add network latency
if (source_subarray != dest_subarray) {
    uint64_t network_cycles = model_subarray_network_transfer(...);
}

// 3. Ramulator Layer:
//    - Maps to BANK (not subarray!)
Request req;
req.addr = physical_address;
req.type_id = Request::Type::Read;
//    - Executes DRAM commands on BANK:
//      ACT <bank>  (activate row)
//      RD  <bank>  (read from row buffer)
//      PRE <bank>  (precharge)
//    - Returns DRAM latency (tRCD + tCAS ~27ns)

// 4. Combined Latency:
total_latency = dram_latency + data_movement_cycles + network_cycles;
```

## Implications

### ✅ What Works Well

**1. Bandwidth Modeling:**
- GSA bandwidth limits are correctly applied (38.4 GB/s DDR4, 64 GB/s HBM2)
- Multiple PEs in same subarray share bandwidth realistically
- Internal network models subarray-to-subarray transfers

**2. Latency Estimation:**
- DRAM access latency from Ramulator (accurate)
- Data movement latency from bandwidth limits (accurate)
- Network latency from internal network model (estimated but reasonable)

**3. Configuration and Control:**
- Can specify subarray-level PE placement
- Can track which PE accesses which subarray
- Can model data locality at subarray granularity

### ❌ What's Approximated

**1. Subarray Conflicts:**
- If 2 PEs in same subarray access different rows → should conflict on sense amps
- Currently modeled as bank conflict (conservative but not precise)

**2. Subarray Row Buffers:**
- Real DRAM may have per-subarray row buffers (not modeled)
- We assume bank-wide row buffer (standard Ramulator model)

**3. Address Mapping:**
- Subarray ID is estimated from row address
- True mapping may be more complex (e.g., interleaved)

**4. Subarray Distance:**
- Network latency assumes fixed 5 cycles within bank
- Real latency depends on physical distance between subarrays

## Recommendations

### For Accurate Subarray-Level PIM Simulation:

#### Option 1: Use Current Model (Recommended for Most Cases)
**Pros:**
- ✅ Correct bandwidth modeling (most important for PIM!)
- ✅ Correct DRAM timing from Ramulator
- ✅ Reasonable network latency estimates
- ✅ Fast simulation

**Cons:**
- ❌ Subarray conflicts approximated as bank conflicts
- ❌ Address mapping simplified

**Use When:**
- Studying bandwidth-limited workloads (most PIM applications!)
- Comparing different DRAM types (DDR4 vs HBM2)
- Analyzing PIM granularity trade-offs

#### Option 2: Extend Ramulator (Future Work)
**What to Add:**
1. Subarray-level address mapping
2. Per-subarray row buffer state
3. Subarray-level timing constraints
4. True subarray conflict detection

**Pros:**
- ✅ Most accurate subarray modeling
- ✅ Precise conflict detection

**Cons:**
- ❌ Significant development effort
- ❌ May not exist in real DRAM (manufacturer-specific)
- ❌ Slower simulation

**Use When:**
- Studying subarray-level conflicts specifically
- Designing new DRAM architectures with exposed subarrays
- Academic research requiring highest accuracy

#### Option 3: Hybrid Approach (Current Implementation)
**What We Do:**
- Use Ramulator for standard DRAM timing (bank-level)
- Add PIM controller layer for subarray-specific features
- Approximate subarray conflicts conservatively

**This is the RIGHT approach for PIM simulation!**

## Validation Checklist

### ✅ Verified Working:
- [x] Subarray granularity defined in PIM payload
- [x] Subarray bandwidth limits from DRAM architecture
- [x] Subarray network configuration (DDR4/HBM2/etc.)
- [x] ALU cores can be configured as subarray PEs
- [x] 256 PEs distributed across 256 subarrays
- [x] Bandwidth tracking at subarray level
- [x] Network latency for cross-subarray transfers

### ⚠️  Approximated:
- [ ] Subarray ID from address (estimated, not true mapping)
- [ ] Subarray conflicts (modeled as bank conflicts)
- [ ] Subarray-to-subarray distance (fixed 5 cycles)

### ❌ Not Implemented (Ramulator Limitation):
- [ ] Native Ramulator subarray commands
- [ ] Per-subarray row buffer state in Ramulator
- [ ] Subarray-level address mapping in Ramulator
- [ ] True subarray conflict detection in Ramulator

## Example Usage

### Valid Subarray-Level PIM Configuration:

```cpp
// 1. Initialize PIM with subarray support
RamulatorWrapper ram("ddr4_config.yaml");
ram.enablePIMSupport("DDR4");

// 2. Register PEs at subarray level
int num_subarrays = 16 * 16;  // 16 subarrays/bank × 16 banks = 256
for (int i = 0; i < num_subarrays; i++) {
    int bank = i / 16;
    int subarray = i % 16;
    ram.registerPE(PIMGranularity::SUBARRAY,
                   pe_id=i,
                   target_bank=bank);
    // Note: subarray ID tracked in PIM controller, not Ramulator
}

// 3. Create subarray-level PIM request
PIMRequestPayload payload;
payload.granularity = PIMGranularity::SUBARRAY;
payload.pe_id = 5;
payload.target_bank = 0;
payload.target_subarray = 5;
payload.data_bytes = 4096;

// 4. Send request (PIM controller handles subarray details)
ram.sendPIM(address, MemoryRequestType::READ, &payload, callback);

// 5. Check results
std::cout << "GSA bandwidth: " << payload.effective_bw_GBs << " GB/s\n";
std::cout << "Data movement: " << payload.data_movement_cycles << " cycles\n";
std::cout << "Network latency: " << payload.network_cycles << " cycles\n";
```

## Conclusion

**Subarray-level PIM is SUPPORTED through the PIM controller layer**, which provides:
- ✅ Subarray bandwidth modeling (GSA limits)
- ✅ Subarray network modeling (within-bank transfers)
- ✅ Subarray-level PE placement and tracking

**Ramulator2 itself does NOT model subarrays**, which is:
- ✅ **CORRECT** - JEDEC specs don't define subarray-level commands
- ✅ **EXPECTED** - Standard DRAM simulators operate at bank level
- ✅ **SUFFICIENT** - Bandwidth limits are the critical factor for PIM!

**The hybrid approach (PIM controller + Ramulator) is the right design** for PIM simulation because:
1. Ramulator provides accurate DRAM timing at bank level
2. PIM controller adds subarray-specific bandwidth and network modeling
3. Most PIM performance is bandwidth-limited (not conflict-limited)
4. This matches how real PIM systems would work (standard DRAM + PIM logic)

## References

- `pimid/memory_models/include/pim_request_payload.h` - Subarray payload definition
- `pimid/memory_models/src/pim_bandwidth_tracker.cpp` - Subarray bandwidth tracking
- `pimid/memory_models/src/internal_dram_network.cpp` - Subarray network modeling
- `pimid/external/zsim/tests/pim_pe_subarray_level.cfg` - Subarray PE configuration
- `ALU_CORE_PIM_VERIFICATION.md` - Comprehensive PIM PE verification
