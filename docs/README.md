# PIMID Documentation

| Document | Covers |
|---|---|
| [architecture.md](architecture.md) | Simulation engine: QEMU + ZSim plugin, methods (exec / trace / trace-gen / synthetic), scopes, two-Garnet hierarchy |
| [memory.md](memory.md) | 11 memory technologies, backend models (Ramulator2 / CACTI / NVSim), PE placement levels, characterization cache |
| [network.md](network.md) | The two NoC models (analytical closed form + detailed Garnet), 8 topologies, DRAM-as-Garnet-tree, synthetic traffic |
| [cores.md](cores.md) | The five PE core models, MLP intensity, ALU scaling factors |
| [cosim.md](cosim.md) | Host-device co-simulation, interconnect link types, MPI semantics |
| [benchmarks.md](benchmarks.md) | 48-benchmark suite + cosim/host kernels, building and running |
| [power.md](power.md) | McPAT power/area integration |
| [build.md](build.md) | Building from source, custom QEMU, no-root/HPC notes, Docker, reproducibility |
| [yaml_reference.md](yaml_reference.md) | Complete configuration key reference |
| [dram_specs.md](dram_specs.md) | Per-technology JEDEC organization mapping |
| [cache_warehouse.md](cache_warehouse.md) | Characterization cache warehouse internals |
