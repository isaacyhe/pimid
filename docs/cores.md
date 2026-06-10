# PE Core Models

Five core models are available for PIM device PEs (`pim.pe.type`) and, in
system scope, for hosts (`hosts[].core_type`) and devices (`devices[].pe_type`).

| Value | ZSim engine | What it models |
|---|---|---|
| `alu_core` | ALU | Stripped-down PIM PE: 1 ALU, no caches. Behavior shaped by scaling factors (below). The default device PE. |
| `simple_core` | Simple | Coarse functional core: IPC = 1 plus serial memory latency. Fast, approximate. Aliases: `Simple`, `simple`. |
| `in_order_core` | InOrder | Cycle-detailed in-order core: two-phase bound/weave timing with contention modeling. Aliases: `InOrder`, `in-order`, `in_order`. |
| `ooo_core` | OOO | Out-of-order superscalar (Westmere-class: 128-entry ROB, 4-issue). Aliases: `OOO`, `OoO`, `ooo`, `out-of-order`. |
| `null_core` | Null | No execution timing — memory/network-only studies. Aliases: `Null`, `null`. |

Any other value is rejected with an error listing the valid names.

## ALU core scaling factors

`alu_core` accepts design-point knobs for processing-using-memory (PUM) and
processing-near-memory (PNM) sweeps:

```yaml
pim:
  pe:
    type: alu_core
    compute_factor: 10.0    # cycles per op (e.g. tRAS-scale for PUM)
    access_factor: 0.0      # memory access cost scale (0 = free local access)
    throughput_factor: 1.0  # issue throughput scale
    operand_width: 1        # bits per operation (1 = bit-serial)
```

## Memory-level parallelism (MLP)

The analytical NoC model divides per-access latency by an MLP intensity `M`
(`noc.mlp`). `M` is a property of the core model, calibrated against the
cycle-accurate `detailed` model; omit `noc.mlp` to use the calibrated default.
See [network.md](network.md).

## Notes

- In-order/OOO cores automatically upgrade a `simple` memory controller to
  `weavesimple` for correct weave-phase interaction.
- Under QEMU user-mode execution there are no decoded micro-ops; the OOO core
  uses a synthetic-basic-block path (1-CPI + memory latency commit model).
