# Example configs

`examples/` holds runnable YAML configs covering every PIMID feature
dimension; they are also shipped in the Docker image as a smoke set.

## Coverage

| Group | Files | Values |
|---|---|---|
| Memory tech (11) | `tech_*.yaml` | DDR3, DDR4, DDR5, LPDDR5, GDDR6, HBM2, HBM3, SRAM, STT_MRAM, PCM, ReRAM |
| Core / PE type (5) | `core_*.yaml` | ooo_core (out-of-order), in_order_core, simple_core, alu_core, null_core |
| NoC topology (6) | `topo_*.yaml` | MESH_2D, CROSSBAR, RING, TORUS_2D, FAT_TREE, BUS |
| Network model (2) | `netmodel_*.yaml` | detailed (cycle-accurate Garnet; the default), analytical (hop-count + M/D/1 + MLP closed form, `noc.mlp` knob) |
| Hierarchical DRAM placement (5) | `placement_*.yaml` | SUBARRAY, BANK, BANK_GROUP, CHIP, RANK |
| HBM logic-die placement (2) | `logicdie_*.yaml` | HBM2, HBM3 |

Multi-process MPI, host/device cosim, trace-gen, and power on/off are
selected via CLI flags, not by extra YAMLs.

Subdirectories: `cosim/` (host-device and multi-device reference configs)
and `integration/` (external-model adapter example -- see
[external_models.md](external_models.md)). Per-benchmark run configs live
with each suite under `benchmarks/` (see [benchmarks.md](benchmarks.md)).

## Run one

```bash
./build/pimid --method exec --config examples/tech_DDR4.yaml
```

## NVSim characterization is slow

The three NVSim-backed techs (STT_MRAM, PCM, ReRAM) run NVSim's first-pass
characterization on each launch -- that takes ~5+ minutes per run. Either
allow long timeouts when smoke-testing them, or run once and reuse the
cached output (see [cache_warehouse.md](cache_warehouse.md)).
