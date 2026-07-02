# PE Core Models

Five core models are available for PIM device PEs (`pim.pe.type`) and, in
system scope, for hosts (`hosts[].core_type`) and devices (`devices[].pe_type`).

| Value | ZSim engine | What it models |
|---|---|---|
| `alu_core` | ALU | Stripped-down PIM PE: 1 ALU, no caches. Behavior shaped by scaling factors (below). The default device PE. |
| `simple_core` | Simple | Coarse functional core: IPC = 1 plus serial memory latency. Fast, approximate. Aliases: `Simple`, `simple`. |
| `in_order_core` | InOrder | Decode-driven in-order pipeline: real RAW-dependency stalls, functional-unit latencies, and dual-issue in program order (no reordering), plus the cross-PE memory-contention weave (`CoreRecorder`). Aliases: `InOrder`, `in-order`, `in_order`. |
| `ooo_core` | Out-of-order | Out-of-order superscalar (Westmere-class: 128-entry ROB, 4-issue). Aliases: `OOO`, `OoO`, `ooo`, `out-of-order`. |
| `null_core` | Null | No timing model: counts instructions (cycles == instrs, IPC = 1) and drops all memory accesses (empty load/store handlers, so no NoC traffic). An IPC = 1 control/upper-bound baseline. Aliases: `Null`, `null`. |

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
    bit_serial: false       # datapath: false = bit-parallel (default), true = bit-serial
    operand_width: 32       # operand bit-width (affects cycles only when bit_serial: true)
    energy_factor: 1.0      # per-op energy scale for reporting only
```

`compute_factor`, `access_factor`, and `throughput_factor` scale the simulated
cycles; `energy_factor` scales reported energy only. `bit_serial` selects the
datapath model and, together with `operand_width`, sets how operand width affects
compute cost:

- **`bit_serial: false` (default -- bit-parallel):** a full-width ALU processes
  any operand in one step, so `operand_width` does **not** affect cycles (it is
  descriptive). This is backward-compatible -- existing configs are unchanged.
- **`bit_serial: true` (bit-serial PUM):** a W-bit operation costs ~W bit-steps,
  so compute cost scales linearly with `operand_width` (`operand_width: 32` is
  ~32x a single-bit op; `operand_width: 1` is the bit-serial primitive). Use this
  to model processing-using-memory (PUM) datapaths; `compute_factor` sets the
  per-bit-step cost.

## Memory-level parallelism (MLP)

The analytical NoC model divides per-access latency by an MLP intensity `M`
(`noc.mlp`). `M` is a property of the core model, calibrated against the
cycle-accurate `detailed` model; omit `noc.mlp` to use the calibrated default.
See [network.md](network.md).

## Fidelity ladder (simple vs in-order vs out-of-order)

`simple_core` is an **IPC = 1** model: one instruction per cycle plus full
blocking memory latency, with no instruction- or memory-level parallelism. It is
the optimistic per-instruction bound and the fast approximation.

`in_order_core` is a decode-driven **in-order pipeline**: it consumes the decoded
uops (below) and issues them in strict program order, stalling on real
per-instruction RAW dependencies, functional-unit latencies, and
issue-width/port contention -- with no reordering. Dependency chains it cannot
hide push it *above* simple's IPC = 1 (e.g. FP-latency-bound stencil/gemv),
while independent work that dual-issue overlaps pulls it slightly *below*; it
also carries the cross-PE memory-contention weave (`CoreRecorder`). Net vs
`simple` it runs roughly -4% to +25% across the kernel suite -- genuinely
distinct, not a rename.

`ooo_core` adds out-of-order issue (128-entry ROB + reordering), hiding latency
the in-order core must stall on, so `ooo <= in_order` on every kernel; the gap
tracks each kernel's ILP/MLP. Use `in_order_core` for dependency/issue-accurate
in-order timing, `ooo_core` for the reordered upper bound, `simple_core` for the
fast IPC = 1 approximation.

## Model boundaries (documented, deliberate)

- **In-order memory is blocking**: one outstanding miss at a time, no
  hit-under-miss MLP. This matches a simple in-order PIM PE and is required by
  the `CoreRecorder` weave's serialization invariant.
- **No wrong-path effects**: a mispredict charges the flush/refill bubble but
  wrong-path fetches do not pollute caches. Standard for simulators of this
  class.
- **Timing microarchitecture is fixed per core model** (`ooo_core`:
  128-entry-ROB/4-wide Westmere-class, compile-time; `in_order_core`: dual-issue
  default). The `power.mcpat_overrides` keys (`pipeline_depth`, `issue_width`,
  ...) shape the McPAT power/area model only, never cycle timing -- see
  [yaml_reference.md](yaml_reference.md).
- At large working sets, `ooo_core` can legitimately exceed `in_order_core` on
  branch-heavy irregular kernels: its mispredict redirect is resolution-bound,
  so a load-fed mispredict pays a DRAM-latency-long refill that a shallow
  in-order pipe does not pay on top of its load-use stall. This is physics, not
  a calibration error.

## Notes

- `null_core` issues NO memory or network traffic -- its load/store handlers are
  empty, so it drops every access. It therefore cannot drive memory- or
  network-only studies (use `alu_core` with `access_factor`/`compute_factor`).
  Its role today is an IPC = 1 control/upper-bound baseline. Note: PIMID does not
  switch core models mid-run, so `null_core` cannot yet fast-forward non-ROI
  regions into a detailed core; true fast-forward (with trace-gen/replay) is
  planned for 1.5.x.
- In-order/out-of-order cores automatically upgrade a `simple` memory controller to
  `weavesimple` for correct weave-phase interaction.
- `in_order_core` reuses the same in-tree x86 decoder as `ooo_core` (below) to
  drive an in-order scoreboard: per-register ready-cycle tracking, dual-issue in
  program order, functional-unit port contention, and load-use stalls (no
  reordering). It also carries the same branch predictor as `ooo_core` (2-level
  PAg), fed with the real per-branch direction; a mispredict charges a 7-cycle
  front-end flush bubble (shallow in-order pipe; override with
  `PIMID_INORDER_MISPRED_PENALTY`, disable with `PIMID_INORDER_NOBRANCH=1`).
  `PIMID_INORDER_NODECODE=1` restores the legacy IPC = 1 path. The issue width
  is configurable via `pim.pe.issue_width` (default 2; env `PIMID_INORDER_WIDTH`
  overrides YAML). Diagnostics per
  in-order core: `uops`, `decodedBbls`, `syntheticBbls`, `depStalls`,
  `issueStalls`, `branches`, `mispredBranches`, `mispredStallCycles`,
  `memMismatchLoads/Stores`.
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
