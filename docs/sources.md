# Sources -- every external number's provenance

One row per externally sourced quantity: what it is, the value or band in
use, the source, and where the code consumes it. Local copies of sources
live in the (unpublished) `misc/` archive of the development tree; entries
below name the archive file where one exists. Rule of the house: a quantity
with units of time/energy/rate/fraction/area is either a spec primitive, a
runtime measurement, or it appears here with a source and (where the
sources disagree) a band.

## DRAM standards (JEDEC)

| Quantity | Value in use | Source | Consumed at |
|---|---|---|---|
| DDR3 timing preset (tRAS/tRP/tBurst overrides) | 35 / 13.75 / 5.0 ns (DDR3-1600K) | JESD79-3D (misc/JESD79-3D.pdf) | `ramulator_wrapper.cpp` per-tech overrides |
| DDR3 termination scheme (R7 rd/wr split) | SSTL-135; read loop RON 34 + RTT_NOM 40, write loop 34 + RTT_WR 120 | JESD79-3D Tables 38, 41 (topology); Micron MT41K p.32 IDD conditions ("RON RZQ/7 (34); RTT,nom RZQ/6 (40); RTT(WR) RZQ/2 (120)") | `pimid_energy.h` termination rows; CACTI-IO injection (rtt1/rtt2 per direction) |
| DDR4 electricals (R7 rd/wr split) | POD12; read 34+40, write 34+120 | JESD8-24 (topology, misc/); Micron MT40A p.315 IDD conditions | CACTI-IO injection (`cacti_io_wrapper.cpp`) |
| DDR5 electricals (R7 rd/wr split) | POD at 1.1 V; read 34+40, write 34+120. NOTE: no "POD11" JESD8-* standard exists (family is POD18/15/135/125/12/10, all in misc/); the 1.1 V point is JESD79-5's own | Micron DDR5 core sheet p.453 IDD conditions (MR5 RZQ/7 drivers, MR35 RTT_NOM RZQ/6, MR34 RTT_WR RZQ/2) | CACTI-IO injection |
| GDDR6 electricals (R7 rd/wr split) | POD135; read 40+60, write 40+120; MR1 default = termination DISABLED, IDD operating point priced | Samsung K4Z80325BC p.166 (driver 40 / termination 60 characteristics), p.144 IDD conditions ("All ODTs are enabled with ZQ/2", ZQ=240), p.49 MR1; JESD250D (topology) | CACTI-IO injection |
| Controller-side write driver RON (DDR4/DDR5) | applied 34 ohm, inside the sourced host range 30-50 ohm | Intel 743844-015 (misc/) Table 87 p.208 (DDR5: RON_UP/DN(DQ) 30-50 ohm, RODT(DQ) 30-240) and Table 86 p.207 (DDR4: 30-50, RODT 40-200); range stated at the row, point chosen within it | `pimid_energy.h` (ron_wr) |
| Controller-side write driver RON (GDDR6) | 40 ohm -- ASSUMED equal to the DRAM's pull-down class (same 240-ohm ZQ reference); no public GDDR6 host-silicon doc in misc/ | the one remaining stated assumption of the R7 split | `pimid_energy.h` (ron_wr) |
| LPDDR5 termination topology | LVSTL, VSS-referenced; DQ ODT default = Disable | Micron datasheets (misc/315b-441b-*.pdf, misc/MICT-S-A0025741931-1.pdf); JESD209-5C Table 84 p.144 (misc/JESD209-5C.pdf) | `pimid_energy.h` (rtt=0 encodes the JEDEC default; sourcing residue N8 CLOSED 1.11.63) |
| LPDDR5 VDDQ | 0.50 V TYP (0.30 V ODT-off; range 0.47-0.57 V) | Micron LPDDR5X y52p p.1 Features; Micron_LPDDR5_MT62F p.1 and Table 7 p.14 | CACTI-IO injection (`cacti_io_wrapper.cpp`), `pimid_energy.h` |
| LPDDR5 driver RON | 40 ohm | Micron IDD-table Note 4 ("Output load = 5pF; RON = 40 ohms; TC = 25 C"): Micron_LPDDR5_MT62F p.15, y52p p.43, y52q p.33, MICT-S p.30, y4bm p.25 | CACTI-IO injection, `pimid_energy.h` |
| LPDDR5 DQ ODT (Rtt) | Disable (Default); ladder RZQ/1..RZQ/6 = 240/120/80/60/48/40 ohm (RZQ=240), 111B RFU; NT-ODT also defaults off | JESD209-5C Table 84 p.144 (misc/JESD209-5C.pdf, acquired 2026-08-24) -- supersedes the 1.11.52-1.11.59 assumption chain (240 ohm assumed from host-side Intel 743844-015 Tbl 89 RODT range) | `pimid_energy.h` (rtt=0, LVSTL branch returns 0 for it), `cacti_io_wrapper.cpp` (row sourced=true); ODT-on systems use power.termination_pj_per_bit |
| HBM3 I/O rails | VDDQ 1.1 V TYP, VDDQL (TX driver output stage) 0.4 V TYP; unterminated (IOUT = 0 mA, Ctotal = 2.5 pF); driver strength specified in CURRENT (8/10/12/14 mA, MR6) not ohms | JESD238B.01 Table 70 p.152 (cl.7.2), cl.9.1 p.164/166, Table 17 p.34 | not injected -- the (vddq, rtt, ron) injection shape has no rtt/ron to give for HBM |
| Power-down entry/exit threshold (E17) | DDR3/DDR4 6.0 ns; DDR5/LPDDR5 7.5 ns | JESD79-3D (tXP max(3nCK,6ns)); JESD79-4; JESD79-5; JESD209-5 | `gapPowerDownResidency()` in `main.cpp`; settable via `memory.power_down_threshold_ns` |
| HBM3 refresh completeness (nRFCSB fill) | tRFC upper bound, stated; JESD238B locally available for the real per-density values (upgrade queued) | JESD238B (misc/JESD238B.01.pdf) | `external/ramulator/.../HBM3.cpp`, `HBM2.cpp` |
| HBM die population minimum | 2 channels per core die (8ch->4, 16ch->8) | JESD235/238 | `memorySystemDieCount()` |
| IDD background/state currents | per-preset IDD2N/IDD2P/IDD3N etc. | JEDEC per-generation + Micron datasheets, rows commented individually | `pimid_energy.h` |
| DRAM device width population (x4/x8/x16 -> chips/rank) | 16/8/4 per 64-bit channel | JEDEC channel arithmetic (spec primitive) | `memorySystemDieCount()`, `backgroundUnits()` |
| Array energy population method | energies x devices-per-access | Micron TN-41-01 method | `devicesPerAccess()` in `pimid_energy.h` |

## Die area / density (measured silicon)

| Quantity | Value in use | Source | Consumed at |
|---|---|---|---|
| DDR4 full-die density | 0.296 Gb/mm^2 (SK Hynix D1z) | TechInsights/SemiAnalysis | `vendorDieDensity()` |
| DDR5 density | 0.315 Gb/mm^2 (Micron D1a, 8Gb/25.41mm^2) | TechInsights | same |
| LPDDR5 density | 16Gb/43.98mm^2 (Samsung D1z) | TechInsights | same |
| GDDR6 density | 8Gb/37.03mm^2 (Samsung K4Z80165BC) | TechInsights floorplan | same |
| HBM3 density | 0.160 Gb/mm^2 (SK Hynix) | SemiAnalysis | same |
| HBM2 density | 8Gb user / 96mm^2 (12x8mm, 20nm) | Sohn et al., ISSCC 2016 18.2 / JSSC 52(1) (misc/sohn2016.pdf, misc/sohn2017.pdf) | same |
| DDR3 density | 4Gb/30.9mm^2 (SK Hynix 23nm) | ISSCC 2012 paper 2.3 press kit | same |
| Array fraction of die | BLSA 8-15%, LWD 5-10% (structure statement) | Vogelsang, MICRO 2010 (misc/vogelsang2010.pdf) | on-pitch boundary rationale (L116/N1) |

## DRAM-process compute (the family + pitch model)

| Quantity | Value in use | Source | Consumed at |
|---|---|---|---|
| Family factors (fa/fd/fl) | derived per run from CACTI .dat hp vs comm-dram columns at the configured node/corner/temperature | CACTI 7 technology tables; the DRAM columns themselves are CACTI-D (Thoziyoor et al., ISCA 2008) | `periphFamilyFactors()` |
| SUBARRAY pitch factor | band [1.25, ~4]; default 1.25 | low end: Samsung FIMDRAM, ISSCC 2021 25.4 (~1.5mm^2/PCU, HBM2/1y, lean SIMD); upper: UPMEM ~10x density claim / family factor (Hot Chips 31, misc/HC31_1.4_UPMEM.FabriceDevaux.v2_1.pdf; process facts in Gomez-Luna et al., arXiv:2105.03814, misc/gomezluna2021_upmem_benchmarking.pdf) | `main.cpp` pitch derivation (N3) |
| DRAM generation feature size F | 1x 19 / 1y 17.5 / 1z 15.5 / 1a 14 / 1b 12.5 nm; cell = 6F^2 buried-wordline | industry generation naming (TechInsights usage); 6F^2 architectural fact | `dramGenFeatureNm()` (reporting) |
| On-die PE process | pinned to generation table (N2 ruling) | measured non-cancellation (this project, gate 1161I5) | config pin + family sites |
| DPU metal-layer constraint | DRAM process ~3 metal layers vs >10 logic | Gomez-Luna et al. quoting UPMEM | pitch band rationale |

## Links / interconnect

| Quantity | Value in use | Source | Consumed at |
|---|---|---|---|
| PCIe gen3/4/5 pJ/bit | gen3 4.0; gen4 1.93 (TX-only) - 6.0; gen5 7.6 - 11.4 | published PHY surveys, per-entry comments | `linkEnergyBandPJPerBit()` |
| NVLink pJ/bit | 1.17 - 1.30 | NVIDIA NVLink4 claims | same |
| UCIe pJ/bit | 0.25 - 0.5 | UCIe consortium figures | same |
| UALink pJ/bit | 3.5 (200G-class SerDes row) | D1 sourcing | same |
| Interposer I/O | 0.3 pJ/bit (app toggle) / 0.80 (50%) | O'Connor et al., MICRO-50 2017 Table 3 (misc/MICRO_2017_Fine_Grained_DRAM.pdf) | MC interposer tier (D2) |
| SerDes per-lane rates | PCIe 8/16/32 GT/s; NVLink4 100G; UALink 200G | PCI-SIG / vendor | `memoryctrl.cc` PIMID block |
| PIPE controller width convention | 32-bit -> gen3 250 / gen4 500 / gen5+CXL 1000 MHz | Intel PIPE spec rev 7.1 | link controller clock |
| Link bandwidth | rate x lanes x encoding (derived, no constant) | spec primitives | `linkBandwidthGBs()` |

## Tools' own models (integrated components)

See the README component table: the seven vendored tools plus THREE
distinct models shipping inside the CACTI tree, each with its own paper and
each load-bearing for PIMID:
- CACTI-IO (Jouppi et al., ICCAD 2012) -- off-chip interface: termination,
  PHY power, IO area.
- CACTI-P (Li et al., ICCAD 2011) -- sleep-transistor / Vcc_min power
  gating; its gated endpoints and retention ratios (periphery 0.35, cell
  0.65 of Vdd) are the authority behind every PIMID power-gating number.
- CACTI-3DD (Chen et al., DATE 2012) -- 3D die-stacked DRAM with a TSV
  model (`external/cacti/TSV.cc`, the CACTI3D paths in `uca.cc`/`io.cc`,
  and a 3D-stacked DRAM sample config). VENDORED AND COMPILED BUT NEVER
  INVOKED by PIMID: nothing in `src/` sets `is_3d_mem`, so our HBM stacks
  are priced today with the interposer I/O band (O'Connor) and a JEDEC
  die-count population, with no TSV area or TSV energy term at all. Same
  shape CACTI-IO had before 1.11.42 harnessed it; logged as an open item
  rather than left silent.
- CACTI-D (Thoziyoor et al., ISCA 2008) -- the DRAM technology extension.
  It supplies the `comm-dram` and `lp-dram` cell/device types, i.e. the
  columns the ENTIRE DRAM-periphery family factor is a ratio against
  (fa/fd/fl = comm-dram vs hp/lstp/lp-dram), the DRAM array model behind
  the JEDEC-calibrated die areas, and the lp-dram column the E2 corner
  ruling maps a low-power on-die PE onto.
Their internal constants are theirs; where PIMID overrides or injects a
value, the row above carries the source, and the injection prints it.

## Local archive

The development tree's `misc/` folder holds local copies of: JESD79-3D,
JESD212C, JESD232A, JESD235B ballout, JESD238B (HBM3), JESD239E, JESD250D
(GDDR6), JESD270-4A, JESD330-4, assorted JESD8-* interface standards, the
Micron LPDDR5X datasheets, Sohn 2016/2017, Vogelsang 2010, O'Connor 2017,
the UPMEM Hot Chips 31 deck, and Gomez-Luna 2021. `misc/` is not part of
the published tree; this file is.
