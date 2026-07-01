# PE Core Models

Five core models are available for PIM device PEs (`pim.pe.type`) and, in
system scope, for hosts (`hosts[].core_type`) and devices (`devices[].pe_type`).

| Value | ZSim engine | What it models |
|---|---|---|
| `alu_core` | ALU | Stripped-down PIM PE: 1 ALU, no caches. Behavior shaped by scaling factors (below). The default device PE. |
| `simple_core` | Simple | Coarse functional core: IPC = 1 plus serial memory latency. Fast, approximate. Aliases: `Simple`, `simple`. |
| `in_order_core` | InOrder | In-order core: IPC = 1 issue plus blocking memory latency plus a two-phase bound/weave pass (`CoreRecorder`) that charges cross-PE memory contention. Aliases: `InOrder`, `in-order`, `in_order`. |
| `ooo_core` | Out-of-order | Out-of-order superscalar (Westmere-class: 128-entry ROB, 4-issue). Aliases: `OOO`, `OoO`, `ooo`, `out-of-order`. |
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

## Fidelity ladder (simple vs in-order vs out-of-order)

`simple_core` and `in_order_core` are both **IPC = 1** models: each retires one
instruction per cycle and blocks on the full memory-access latency (no
instruction-level or memory-level parallelism). Only `ooo_core` consumes the
decoded uops (below) to model ILP/MLP.

The sole difference between `in_order_core` and `simple_core` is the in-order
core's second bound/weave pass (`CoreRecorder`), which accounts for **cross-PE
memory contention**. When that contention is negligible -- e.g. PEs placed on
disjoint memory units with private working sets -- the two produce essentially
identical cycle counts; `in_order_core` pulls ahead of `simple_core` only when
PEs actually contend for shared memory datapaths. Neither models per-instruction
in-order pipeline effects (dependency/issue stalls); for dependency- and
issue-accurate timing, use `ooo_core`.

## Notes

- In-order/out-of-order cores automatically upgrade a `simple` memory controller to
  `weavesimple` for correct weave-phase interaction.
- Under QEMU user-mode execution the plugin decodes each guest x86 instruction
  into ZSim `DynUop`s (register read/write sets, latency class, functional-unit
  port, load/store markers) with a minimal in-tree x86-64 decoder
  (`external/zsim/src/x86_decoder.h`). This drives the out-of-order core's
  dependency-driven pipeline (128-entry ROB, 4-wide issue, port contention,
  load/store queues, register scoreboard) instead of the old synthetic 1-CPI
  path, so `ooo_core` now models ILP and memory-level parallelism: on
  compute-bound kernels it retires faster than `in_order_core` (2.0-2.4x on
  streaming/gemv, 1.3-1.4x on irregular bfs/stencil under HBM3/16-PE/detailed-NoC;
  the gap tracks the kernel's ILP/MLP). The decoder covers the common
  integer/SSE/SSE2/AVX forms precisely (full integer ALU incl. group-1 immediate
  arithmetic and shifts, imul/idiv, load-op/rmw, lea, push/pop/call/ret, mov-imm,
  movzx/movsx/movsxd, jcc/cmov/setcc, SSE/SSE2 scalar+packed FP add/mul/div/sqrt/
  cvt/compare, packed-integer logic/add/sub/mul/shift/shuffle/pack, pmovmskb,
  bsf/bsr, and lock-prefixed atomics/cmpxchg/xadd/xchg as fenced rmw), leaving
  only ~0.1-0.5% of dynamic instructions on a generic-uop fallback (integer div's
  rdx:rax pair, rep-string, syscall/cpuid). Unrecognized instructions' memory
  accesses are absorbed by a tolerant load/store drain, so counts never desync
  (measured memMismatch is <0.05% of memory ops). The out-of-order branch predictor is
  driven by the per-branch direction (resolved from the actual next TB) so
  mispredicts incur the correct front-end penalty. Diagnostics per out-of-order core:
  `uops`, `decodedBbls`, `syntheticBbls`, `approxInstrs`, `mispredBranches`,
  `memMismatchLoads/Stores`. Env toggles: `PIMID_OOO_NODECODE=1` forces the legacy
  synthetic path; `PIMID_OOO_NOBRANCH=1` disables branch-predictor feed;
  `PIMID_OOO_DUMP=1` profiles which opcodes hit the generic fallback (weighted by
  dynamic count); `PIMID_OOO_DEBUG=1` traces the first decoded BBLs. Other core
  types ignore the decoded uops and never receive branch callbacks, so their
  timing is byte-for-byte unchanged.
