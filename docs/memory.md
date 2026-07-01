# Memory Technologies

PIMID models 11 memory technologies through three production backends:

| Technology | Backend | Notes |
|---|---|---|
| DDR3, DDR4, DDR5 | Ramulator2 | JEDEC-verified presets |
| LPDDR5, GDDR6 | Ramulator2 | |
| HBM2, HBM3 | Ramulator2 | HBM3 = 16 channels, HBM2 = 8 |
| SRAM | CACTI 7.0 | arrays + cache timing/area |
| STT-MRAM, PCM, ReRAM | NVSim | first run characterizes (~5-7 min), then cached |

Per-technology JEDEC organization (channels, banks, timings) drives both the
memory timing and the device-internal network shape — see
[dram_specs.md](dram_specs.md) and [network.md](network.md).

## PE placement levels

PEs attach inside the memory hierarchy via `pim.placement.level`. The levels,
finest to coarsest, are `SUBARRAY`, `BANK`, `BANK_GROUP`, `RANK`, `CHANNEL`,
`LOGIC_DIE`, plus `HOST_MC` (PEs share the host memory controller). `CHIP`
exists as a virtual layer but is degenerate -- not a distinct placement tier.

Which levels are valid is **per technology**: a tier must be real silicon in
that device's JEDEC organization, so the coarse anchor differs by device class
-- DDRx is rank-centric, LPDDR5/GDDR6 are channel-centric, and only HBM has an
in-package logic die.

| Technology | Valid placement ladder (fine -> coarse) |
|---|---|
| DDR3 | `SUBARRAY -> BANK -> RANK` (no bank groups) |
| DDR4, DDR5 | `SUBARRAY -> BANK -> BANK_GROUP -> RANK` |
| LPDDR5, GDDR6 | `SUBARRAY -> BANK -> BANK_GROUP -> CHANNEL` |
| HBM2, HBM3 | `SUBARRAY -> BANK -> BANK_GROUP -> CHANNEL -> LOGIC_DIE` |

Finer placement = more PEs, each local to a smaller slice, and a deeper, more
parallel device network. Coarser placement funnels more PEs through shared
datapaths, so they contend more. The detailed model charges this contention for
real -- including cross-rank contention under message-passing (see
[network.md](network.md)).

When multiple PEs are placed at a level and no explicit `pim.pe_mem_map` is
given, PEs are distributed **distant/strided** across that level's units -- each
PE local to its own evenly-spaced slice (e.g. 4 PEs over 512 banks land on banks
0, 128, 256, 384), never clustered at the front. This keeps per-PE working sets
disjoint and the load balanced; an explicit `pe_mem_map` overrides it.

## Characterization cache warehouse

Expensive deterministic backend characterizations (NVSim's multi-minute design
search) are memoized in a central warehouse with `rw/ro/wo/off` modes and an
inspectable manifest: a technology is characterized once and reused by every
later run and sweep process. Details: [cache_warehouse.md](cache_warehouse.md).
