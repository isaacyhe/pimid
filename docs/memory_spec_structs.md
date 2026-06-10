# Memory architecture specification structs (C++ API)

`src/memory/` ships a set of header-only architecture specifications used by
PE placement, bandwidth tracking, and the memory models. They describe the
*internal* structure of each memory technology: hierarchy organization, port
bitwidths at every level, timing, and data-movement energy.

Not to be confused with [dram_specs.md](dram_specs.md): that page documents
the **Ramulator2 presets** that drive cycle-level DRAM timing. The structs
here capture the architectural parameters (especially internal port widths)
that Ramulator2 does not model.

## Why internal port bitwidths matter

Inside DRAM chips, banks and bank groups have **narrow internal ports**
(8-16 bits for DDR), not the wide external interfaces (64+ bits) seen at the
DIMM level:

```
DDR4/DDR5:  8-16 bit bank ports   (narrow) -> rank-level PIM optimal
HBM2/HBM3:  64-128 bit bank ports (wide)   -> bank-level PIM viable
```

This is the single biggest architectural factor for where PIM processing
elements should be placed. DDR's planar wire routing limits practical
internal ports to 8-16 bits; HBM's through-silicon vias (TSVs) enable wide
internal paths, which is why fine-grained (bank/subarray) PIM is viable on
HBM but not on commodity DDR.

## Files

| File | Purpose |
|------|---------|
| `dram_architecture.h` | DRAM specs: `createDDR4_2400()`, `createDDR5_4800()`, `createHBM2()`, `createHBM3()` |
| `dram_architecture_v2.h` | Same techs with inner-bank timing breakdown (`*_Verified()` creators) |
| `sram_architecture.h` | SRAM (cache) specs with inner-bank timing |
| `sttmram_architecture.h` | STT-MRAM specs with read/write asymmetry |
| `pcm_architecture.h` | PCM (phase-change memory) specs |
| `reram_architecture.h` | ReRAM/memristor specs with analog-compute support |
| `dram_architecture.cpp` | Implementation (`printSummary()` etc.) |
| `dram_config_example.json` | Example config for hypothetical-architecture studies |

The structs cover four explicit DRAM creators; the other simulator
technologies (DDR3, LPDDR5, GDDR6 -- see [memory.md](memory.md)) take their
cycle timing from Ramulator2 presets and their organization from the
architecture extractor.

## Quick start

```cpp
#include "memory/dram_architecture.h"
using namespace pimid::memory;

auto ddr4 = createDDR4_2400();
ddr4->printSummary();

std::cout << "Bank port: " << ddr4->ports.bank_port_bits << " bits\n";
std::cout << "Bank BW:   " << ddr4->getBankBandwidth() << " GB/s\n";

auto hbm2 = createHBM2();
std::cout << "HBM2 bank port: " << hbm2->ports.bank_port_bits << " bits\n";  // 64
```

## Structure

**Ports (`DRAMPortBitwidths`)** -- bitwidth at every hierarchy level:
`subarray_port_bits`, `bank_port_bits`, `bank_group_port_bits`,
`chip_internal_bits`, `chip_io_bits`, `rank_data_bits`,
`channel_data_bits`, plus a `port_width_scale` factor for hypothetical
studies. Methods: `getBankBandwidth()`, `getRankBandwidth()`,
`applyScaling()`.

**Organization (`DRAMOrganization`)** -- physical hierarchy and capacity at
each level (subarrays per bank, banks per bank group, bank groups per chip,
chips per rank, per-level capacities).

**Timing (`DRAMTiming`)** -- standard DRAM timing (tRCD, tCAS, ...) plus
hierarchical access latencies (subarray / bank / chip / rank access). The v2
header additionally breaks down the inner-bank datapath hidden inside tCAS
(column decoder, mux, output driver, H-tree segments, global I/O) -- e.g.
~6.65 ns total for DDR4 vs ~3.05 ns for HBM2, because TSVs shorten internal
paths.

**Energy (`DRAMEnergy`)** -- data-movement energy per byte at each hierarchy
level (e.g. DDR4: ~1 pJ/B subarray, ~2 pJ/B bank, ~10 pJ/B rank,
~15 pJ/B channel).

## Hypothetical scalability studies

The `port_width_scale` knob answers questions like "at what internal port
width does bank-level PIM become viable on DDR?":

```cpp
auto arch = createDDR4_2400();
arch->ports.port_width_scale = 4.0;   // 4x wider internal ports
arch->energy.energy_scale = 1.0 + 3.0 * 0.3;  // energy grows sub-linearly
arch->applyScaling();
// bank port: 8 -> 32 bits; bank BW scales accordingly
```

Observed in our studies: DDR needs roughly 8x wider bank ports before
bank-level PIM matches rank-level PIM, and energy grows sub-linearly with
port width (4x width -> ~1.5x energy).

## Specification sources

Timing and organization values come from JEDEC standards (JESD79-4/-5,
JESD235A, JESD238) and vendor datasheets. External I/O widths are exact;
internal port bitwidths are not published, so they are estimates based on
industry knowledge and bandwidth measurements (treat them as ~representative,
not vendor-exact). Background reading: SALP (ISCA'12), Tiered-Latency DRAM
(HPCA'13), FIGARO (MICRO'20) for DRAM internals; ISAAC and PRIME (ISCA'16)
for ReRAM analog PIM.
