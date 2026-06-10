# Power and Area

McPAT provides per-component power/area: cores, caches, memory controllers,
and NoC. CACTI supplies SRAM/cache geometry; NVSim supplies NVM cell
characteristics; DRAM energy derives from Ramulator2 activity.

- Power analysis is on by default; disable with `--no-power` for fast timing
  sweeps.
- Output reports per-component breakdown (W) and total energy (nJ) alongside
  the timing results.
- `pim.pe.energy_factor` scales per-op PE energy in reports (timing
  unaffected).
