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

PEs attach at one of six hierarchy levels (`pim.placement.level`):
`SUBARRAY`, `BANK`, `BANK_GROUP`, `CHIP`, `RANK`, or `HOST_MC`.
Finer placement = more PEs and a deeper device network.

## Characterization cache warehouse

Expensive deterministic backend characterizations (NVSim's multi-minute design
search) are memoized in a central warehouse with `rw/ro/wo/off` modes and an
inspectable manifest: a technology is characterized once and reused by every
later run and sweep process. Details: [cache_warehouse.md](cache_warehouse.md).
