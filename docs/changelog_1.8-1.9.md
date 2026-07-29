# Changelog / defect ledger -- 1.8.0 -> 1.9.23

Release + defect ledger for the co-sim MPI window, the measured-feedback MPI
pricing model, and the OMP critical-path metric. One entry per release; each
gives the defect fixed, a one-line root cause, and a data-impact note (which
sweep generations the fix invalidates or corrects). Authoritative source is the
release commit messages; deeper design rationale for 1.9.0 is in
`docs-dev/DESIGN_190_PDES.md`.

## 1.9.23 -- revert 1.9.21 (its device-scope regression outweighed its host fix)

1.9.21 changed how a rank's transport wait is accounted for. It fixed a real
defect on the co-simulation host, where a single core is shared by every rank
and the previous rule discounted co-resident ranks' execution along with the
wait. It also introduced a worse defect on the device.

The wait correction is a PAIR. Waking a parked rank, the simulator does not set
its clock to when its message or barrier released it -- it snaps the clock to
the global phase clock, wherever the simulation as a whole has reached. That
value depends on how far every other simulator thread progressed meanwhile, so
it imports the running machine into the simulated timeline. The correction
reverts that snap and then charges the real wait, computed from arrival times.
1.9.21 removed the revert and left the charge, so the snap survived AND the
computed wait was added on top. Device-scope cycles inflated by roughly a
factor of three, consistently, across a ten-run gate.

An intermediate attempt restored the revert but applied it only where a core
was provably the rank's alone, detecting shared use with a per-core bind
counter. That counter incremented on every bind, including a rank rebinding to
its own core after a barrier, so each rank tripped its own guard and the revert
was skipped almost everywhere. The device-scope figure did not recover.

This release reverts 1.9.21 in full. The accounting sources are restored
byte-for-byte to their 1.9.20 state, and the device-scope determinism gate
returns to its pre-1.9.21 values across all ten runs.

The host-side defect that 1.9.21 set out to fix is therefore STILL PRESENT: on
a shared host core the wait correction discounts other ranks' execution, and a
co-simulation cell with a substantial host phase can report an implausibly high
instructions-per-cycle rate. That is a known open defect, tracked with the
maintainers, and it is the lesser of the two problems -- it affects the host
phase of co-simulation cells, whereas the regression affected every
device-scope result.

The durable fix is not another guard on the difference. It is to set the clock
to the computed wake-up time directly, rather than correcting a snap after the
fact: an absolute write cannot discount another rank's work, because it is not
a subtraction, and it makes the shared-versus-exclusive distinction irrelevant.
That requires rebasing the core's pending weave state, since the simulator's
clock is monotonic, and is scoped as its own release.

## 1.9.21 -- follow the CORE: never subtract another thread's work

Defect: two runs of the same co-simulation cell retired the same instructions
but reported host cycle counts differing by nearly two orders of magnitude, one
of them implying an instructions-per-cycle rate far above the core's issue
width and therefore impossible. Reported energy followed the cycle count, so it
diverged with it.

Root cause. `cycles` is a PER-CORE counter, while ranks and threads are
software constructs. At MPI_COMM_END the plugin rewound the entire
COMM_BEGIN->END delta of that per-core counter as though it were the calling
thread's own transport wait. That is valid only when the thread has the core to
itself. In co-simulation the host is a single core shared by every rank, so the
delta also contains co-resident ranks' execution, and subtracting it removed
real work -- deflating the core rather than discounting a wait.

The sharing was confirmed directly with the diagnostics below: an affected
window showed an unchanged core id, an unchanged Core object and an unchanged
domain, with a second thread resident on the same core. Nothing had migrated.

Fix: follow the core. The reported count is what that core advanced by
SIMULATING WORK, whoever ran on it. Only clock JUMPS are subtracted -- the
parked rejoin fast-forward and the cSimStart/cSimEnd weave resolutions, which
are simulator artifacts rather than execution. They are accumulated per core in
Core::pimidJumpCycles and removed at the reporting site. Work is never
subtracted. The per-thread COMM-window rewind is deleted.

This holds at every subscription ratio, which the earlier attempts did not.
Oversubscribed, the counter legitimately aggregates every thread that ran on
the core and nothing is taken away. Undersubscribed, an idle core never
advances and contributes nothing. Equal, behaviour is unchanged.

Three earlier attempts failed and are recorded so the dead ends are not
re-explored. Rewinding only measured clock jumps left the device scope
inflated, and bimodal within a single binary, because it also dropped the
legitimate corrections. Gating accrual inside the comm window was rejected
before implementation: the two runs retire the same total instructions while
differing greatly in how many fall inside windows, so those are the SAME
instructions misattributed, and gating would have erased real ones. A same-core
guard keyed on the core id passed several runs and then failed, because the
1.9.20 PE pinning returns a rank to its own home PE, so the core id no longer
distinguishes a shared core from an exclusive one.

Validation was PARTIAL when this was committed. One co-simulation cell on the
configuration that previously collapsed returned to a plausible
instructions-per-cycle rate. Further repeats and the device-scope determinism
gate were still queued; the gate is the load-bearing check, since it
distinguishes a correct fix from one that has stopped deflating and begun
inflating. Measured evidence is kept with the maintainers.

Data impact: co-simulation host cycles and host energy change on every cell
with a real host phase, and device-rank cycles change wherever an ALU core was
previously rewound -- ALUCore::pimidRewindCycles moves curCycle itself, so that
rewind corrupted the simulated timeline rather than only the report. Results
are NOT comparable with earlier generations.

Diagnostics, default off: PIMID_COMM_DIAG=1 prints a per-thread census of comm
windows -- their count, what the previous rule would have rewound, what was
actually rewound, and how many instructions retired inside each window. A large
would-be rewind against a nonzero in-window instruction count on a shared core
is the signature of this defect.

## 1.9.20 -- deterministic rank-to-PE placement under thread-MPI (defect #15, partial)

Defect: which device PE a thread-MPI rank landed on was decided by a race, so
two runs of the same cell placed the same work on different PEs.

Root cause. Half the mapping was already deterministic: PE index to Garnet node
is fixed at init (`mems[i]` carries `mcId_ = i`; `PEMemoryInterface` computes
`srcNode = mcId_ % numNodes`). The thread-to-core half was not.
`Scheduler::schedThread` first re-takes the thread's last context if it is still
IDLE -- a race against whoever else wants it -- and otherwise takes
`freeList.front()`, where the free list is ordered by the wall-clock sequence in
which cores were released. Ranks cross that path on every host-to-device
migration, so each run drew a different rank-to-PE permutation. Same work,
different placement, hence different hop distances and different latencies.

Fix: each rank now gets a single-PE affinity mask (its "home"), so
`schedThread` has no other candidate to race for; its selection algorithm is
untouched. The key is `vcpu_index`, NOT `tid`: `tid` is handed out `nextTid++`
on first callback, i.e. in arrival order, so it is raced itself and pinning by
it would only have relabelled the same nondeterminism. Homes are permanent for
the run, so a rank that returns to the host and migrates back reclaims the same
PE. A collision guard falls back to the old unpinned mask if a non-bijective
vcpu set would put two ranks on one PE, which would otherwise deadlock them
against each other across a barrier. `PIMID_NO_PE_PIN=1` disables the whole
thing. Non-MPI (OpenMP, device-only) paths are untouched.

Validated -- placement determinism: PASS. Two co-sim runs on different node
types now put the heavy rank on `src=0` with every PE in the same slot; before
the fix the heavy PE moved from 4 to 10 between the same two runs.

Validated -- run-to-run cycle spread: NO IMPROVEMENT. This is a negative
result and it is stated as one. Paired arms in one job on one node, device
scope, HBM3 detailed stencil_2d 256, differing only by `PIMID_NO_PE_PIN`:

Paired arms in one job on one node, differing only by PIMID_NO_PE_PIN, showed
no reduction in spread: the unpinned arm was if anything marginally tighter,
and the variance ratio was far from significant at this sample size. Measured
values are kept with the maintainers.

The unpinned arm is if anything tighter; the variance ratio is 0.72 where
F(4,4) needs ~6.4 for p=0.05. The co-sim BFS cell agrees: n=5 after the fix
gives a spread statistically indistinguishable from the 1.9.19 baseline,
a variance ratio of 1.27. The placement race was real, but it was not what
drives the cycle variance.

What defect #15 actually is, restated from the same 10 runs:

A single rank's own cycle count is very nearly reproducible, while the
REPORTED figure -- a maximum over all ranks -- is several times noisier.
Measured values are kept with the maintainers.

A single rank simulates very nearly reproducibly. The reported device-cycle
figure is a max over 16 ranks -- an order statistic that amplifies small
per-rank jitter, and whose critical rank alternates (rank 0 was slowest in 3 of
10 runs, and close behind the slowest in the rest). The residual therefore
lives in the cross-rank critical path, not in any rank's own simulation. It is
not a data-dependence artifact either: stencil_2d is a regular kernel and
behaves like BFS here. Compare 1.6.3, where the weave quantum was concluded to
be PDES-fundamental.

Defect #15 REMAINS OPEN. This release removes one confirmed source without
closing it.

Data impact: pinning changes placement relative to the previous raced
assignment, so 1.9.20 device and co-sim results are NOT bit-comparable with
earlier generations. The change is within the run-to-run spread above, but it
is a real change of placement, not noise.

Also in this release, diagnostics only, default off: `PIMID_INJ_DUMP=<path>`
writes a per-source injection census (count, destination sum, cycle sum) at
SimEnd. This is what identified the permutation above and is the regression
test for it.

## 1.9.19 -- docs-only: how 1.9.17 and 1.9.18 were validated

Bit-identical timing was the intended acceptance test for both, and it could
not be run: the message-passing co-simulation path is not reproducible run to
run (defect #15, open). Five repeats of the UNCHANGED binary on one cell
(HBM3, message-passing BFS) span a few percent of device cycles. An earlier
two-run estimate was too small a sample to see this.

The changes were therefore validated against that noise: five repeats per
binary, balanced across two node types so machine effects fall on both arms.

Five repeats per binary, balanced across two node types so machine effects
fall on both arms. The means differ by well under one standard deviation and
the ranges overlap almost entirely: no evidence the changes move results.
Measured values are kept with the maintainers.

The means differ by well under one standard deviation, and the ranges overlap almost
entirely (before 293.25M-301.73M, after 293.60M-300.68M). No evidence the
changes move results. Flush cycles were bit-identical across all ten runs and
host cycles varied negligibly and equally in both arms, so the
deterministic parts of the accounting are untouched.

What is established directly rather than statistically: the hang is gone. With
the fix the cut advances ~1M per fold instead of freezing, folding proceeds,
and the pending set stays near 1.1M records instead of passing 64M.

State the claim as "indistinguishable from baseline within the tool's measured
reproducibility", NOT as "verified identical". It cannot be verified identical
until #15 is fixed.

Follow-up, same instrumentation: two runs of one binary dumping per-fold
membership checksums (`PIMID_DET_EPOCH_DUMP`) diverge in MEMBERSHIP on 100% of
shared phases, starting at phase 0, and in MEASUREMENT on 0% -- there is no
phase where the same record set yields a different latency. The Garnet timing
model is deterministic; what varies is which records reach it.

## 1.9.15 -- docs-only

Documentation for 1.9.12 through 1.9.14 (the entries below, plus the memory
and power notes they touch). No source change; no data impact.

## 1.9.14 -- CACTI area units and quiet latency-only queries

Reporting-only; no model consumes either value, and every consumer found by
inspection is a print statement. No data impact.

- **Area units.** CACTI returns areas in um^2 and cache height/width in um;
  the wrapper forwarded them unchanged while labelling them mm^2 and mm, so
  every reported area was inflated by 10^6 (a 64 KB SRAM bank printed as
  6.8e4 mm^2 instead of 0.068 mm^2). Converted in `getArea`,
  `getCacheHeight`, `getCacheWidth`, `getSubarrayArea`, `getCellArea`.
  Access and cycle times were already correct (CACTI returns seconds; the
  wrapper treats them as seconds).
- **Quiet flag.** The cache-latency helper instantiates CACTI purely for an
  access time, but the shared initializer printed a full banner including
  energies that path never consumes, so logs carried a placeholder-looking
  2 MB / 1 nJ block beside the real per-technology numbers. `SRAMConfig::quiet`
  suppresses the banner for latency-only queries.

## 1.9.13 -- device write accounting: GETX is a write at cacheless PEs

- **Defect #17 -- device stores counted as reads.** The PE memory interface
  classified accesses by coherence request type, counting GETX as a read.
  Correct for a cached requester (the store's memory write appears later as a
  PUTX writeback) but wrong for cacheless device PEs, whose stores arrive as
  GETX with no writeback to follow: every device write was recorded as a read
  and the write counter stayed zero. GETX now increments the write counter at
  that interface; PUTS (clean writeback) remains a non-access.
- **Locality counters folded into read/write totals.** The stats aggregator
  added `localAcc`/`remoteAcc` into `mem_rd`/`mem_wr`. Those are a
  where-split of the same accesses, not a read/write split, so each access
  was charged once as a read and once as a write. The aggregator now uses
  only the true rd/wr counters.
- **Data impact.** Device-scope energy for all generations before 1.9.13:
  read/write mix mispriced (all-reads at the interface, plus the
  double-charge). Timing unaffected. The error is first-order where write
  energy dwarfs read energy -- at the swept 64 KB bank a PCM write costs
  orders of magnitude above a DRAM read per line -- and second-order on DRAM,
  whose read and write burst energies are comparable. Validated by
  measurement: with the fix, total accesses (rd+wr) reproduce the pre-fix
  totals negligibly, i.e. the same traffic correctly labelled, and recovered
  write fractions match kernel semantics (STREAM triad worker PEs at exactly
  1/3).

## 1.9.12 -- NVM per-access width fix + NVSim cache-key hardening

- **Defect #17a -- per-access width.** NVM characterizations were requested
  with `word_width_bits` left at a single 64-bit word while accesses are
  full 64 B lines, undercharging NVM array energy by about 8x and shifting
  modelled latencies (PCM read 1.127 -> 2.832 ns at the swept bank). The
  power path now sets `word_width_bits = cache_line_size * 8` and the three
  timing sites request 512 b, matching the CACTI/SRAM path.
- **Cache-key hardening.** The NVSim characterization cache keyed on
  (type, capacity, process node) without the word width, so pre-fix and
  post-fix entries could alias; the key and the on-disk filename now include
  it (`..._w512.xml`), and capacities print in KB.
- **Data impact.** All NVM (STT-MRAM/PCM/ReRAM) device energies before
  1.9.12 are invalid; corrected per-64 B constants at the swept 64 KB bank
  differ per medium, with PCM's write cost dominating the set, in nJ
  (read/write). Cycle counts shift by a few percent where array timing
  matters and are unchanged where the network dominates.

## 1.9.11 -- docs-only

Documentation for the 1.9.10 energy-model overhaul (this entry, the yaml
reference knob ladder, and the energy-model notes below). No source change;
no data impact.

## 1.9.10 -- energy-model overhaul: system-scope integration fix + tool-measured memory energy

Eight commits; device-scope TIMING is bit-invariant (same-node A/B gate on the
release binary: cycle counts agree to within a rounding-level delta, both rc=0).

- **Defect #16 -- system-scope power integration.** `runPerNodePowerAnalysis`
  priced every node over the first host core's contention-EXCLUDED unhalted
  cycles while feeding full aggregate access counts, producing nonphysical
  system powers (implausibly high bfs baselines; kW-class co-sim host nodes; below-idle
  cells). Fix: each node is priced over true wall-clock time in its own clock
  domain (host wall = max(unhalted + contention) across host cores; device
  wall = max device cycles). Device-scope `runPowerAnalysis` is a different
  function and is unaffected.
- **Configurable process node.** `power.tech_node_nm` (+ `device_`/`host_`
  variants); the host now inherits the device node. Finding: the old hardcoded
  7 nm host literal never reached McPAT (a `max(22, n)` clamp), so all landed
  data was already effectively 22 nm; the change is forward-looking, not
  retroactive.
- **Ramulator2 energy layer, tool-measured.** Root cause of the 0.000-nJ
  energy reports: a never-fed counter behind a cycle-0 guard, plus "INFERRED"
  placeholder `bank_energy_pJ` constants. The energy layer now lives inside
  Ramulator2 (`external/ramulator/src/dram/pimid_energy.h`): JEDEC IDD/VDD
  per-command energies with first-class `current_presets` for all seven DRAM
  standards (DDR3/LPDDR5/GDDR6/HBM2/HBM3 added; the DDR4-class reuse fallback
  is retired), background/refresh power from standby currents, and per-scheme
  termination/ODT (SSTL/POD/LVSTL): DDR3 17.6 / DDR4 9.4 / DDR5 5.25 /
  GDDR6 2.6 / LPDDR5 0 / HBM 0 pJ/bit. The wrapper is a thin reader; the
  relocation was verified value-invariant to the pre-migration table.
- **Off-chip channel, die-boundary split.** CPU-side McPAT MC/PHY (~9 pJ/bit
  at 22 nm) + DRAM-side I/O (0.76) + termination (5.25) = ~15 pJ/bit for DDR5,
  inside the published 15-22 pJ/bit full-channel band; HBM carries no
  termination (interposer), a physics-derived asymmetry.
- **Known boundary.** The device H-tree fabric is priced as a McPAT bus-mode
  wire/repeater datapath (7.3 mW for a 16-PE tree) in the analysis layer that
  produced the published dataset; the binary's own per-node NoC printout still
  uses router-mode pricing, and a runtime `power.noc_model` knob is roadmap
  work, not shipped in this release.
- **Data impact.** Timing: none (gate above). System-scope powers/energies
  produced before 1.9.10 are nonphysical and were re-derived; device-scope
  energies were re-derived onto the measured per-command constants. The
  release's knobs are documented in `docs/yaml_reference.md`.

## 1.9.9 -- docs-only

Documentation for 1.9.8 (this entry, badge). No source change.

## 1.9.8 -- power-derivation fixes (1-PE NoC gating; subarray fan-in overcount)

- **Defect 1.** runPowerAnalysis gated the in-memory-network power on
  num_pes > 1, dropping the H-tree leakage term for every 1-PE device run.
  Fixed: gate also fires on hierarchy_enabled. 1-PE
  pecount power/energy re-derived from existing logs (no re-simulation);
  multi-PE cells bit-identical.
- **Defect 2.** buildNoCLevelsForMcPAT used a hardcoded x2 subarray->bank
  fan-in instead of config.subarrays_per_bank (32 for HBM3), overfeeding
  McPAT ~16x router counts at SUBARRAY placement only (65 W vs a physical
  term). Fixed; SUBARRAY placement rows re-derived; other levels
  bit-identical.
- **Documented (no code change).** Power templates key off microarch class,
  not timing fidelity: simple_core and in_order_core share the single-issue
  template. total_power_W is leakage/config-dominated; activity
  moves only trailing digits; Ramulator2 array dynamic energy is reported
  separately.

## 1.9.7 -- docs-only

Documentation for 1.9.6 (this entry, badge). No source change.

## 1.9.6 -- thread-MPI head-of-line deadlock at >16 ranks

- **Defect.** The in-process MPI transport's per-rank mailbox ring (16 slots)
  deadlocked under source-matched receive when >16 ranks flooded a collective
  root: the receiver would not consume the ring head (wrong source) and the
  wanted sender could not append (ring full). bfs (per-level gathers) at
  32/64 ranks froze within ~5 phases; 8/16 ranks and one-shot-reduce kernels
  never filled the ring. Latent since 1.6; exposed when measured pricing
  (1.9.0) changed rank arrival patterns.
- **Fix.** Unexpected-message staging queue per receiver: when the ring is
  full and the wanted source absent, the head message is staged (freeing the
  slot, waking the sender). Source matching, per-source FIFO, and consumed
  timestamps unchanged -- deadlock-free by construction, deterministic.
  Capacity is now elastic at any rank count.
- **Data impact.** pc_32/pc_64 MPI bfs cells producible (v196 tags); all
  other cells gate-verified unchanged (all within their documented bands,
  cosim clean with exact 16x flush arithmetic).

## 1.9.5 -- docs-only

Documentation for 1.9.4 (this entry, cores.md note, badge). No source change.

## 1.9.4 -- simple_core phantom wall-clock leak under thread-MPI

- **Defect.** simple_core's frozen-clock rewind (COMM_END) lowered curCycle
  directly, but SimpleCore::join() re-pins curCycle to the wall-pumped global
  phase clock on every phase crossing -- the rewind did not stick. Rank clocks
  tracked wall time instead of work; on rendezvous-heavy kernels the phantom
  dominated (bfs: 34.6M of 36.9M cycles). Same family as the 1.9.2 OOO defect:
  per-class rewind paths unsafe against clock re-pinning.
- **Fix.** Accumulate the rewind in pimidPhantomWait and net it out at the
  read sites (getCycles / ROI stat) -- join-immune, byte-inert outside MPI
  frozen-clock waits. simple_core-only; other cores + OMP gate-verified
  unchanged.
- **Data impact.** simple_core MPI cells re-run (v194): bfs 36.9M -> 8.37M
  (physical: between ooo 3.8M and alu 24.5M, just above in_order 8.1M).

## 1.9.3 -- docs-only

Documentation for 1.9.2 (this entry, cores.md OOO bulk-advance note, version
badge). No source change; no data impact.

## 1.9.2 -- window-safe bulk clock advance for OOO cores (1.6.3 boundary closed)

- **Defect.** Under thread-MPI rendezvous, `OOOCore::join()` applied a raw
  `curCycle = targetCycle` jump while parked, bypassing the window-safe
  `longAdvance()`. A 100K-1M+ cycle rendezvous jump orphaned in-flight
  unbounded-window entries behind `curCycle`; the next window rebase computed a
  negative position and tripped the `ooo_core.h` assert (then SIGSEGV). bfs
  (one collective per frontier level) is the reliable trigger; this is the
  1.6.3 "weave quantum" boundary that had kept OOO+thread-MPI cells pulled.
- **Fix.** Route the parked join through `insWindow.longAdvance()` (drain-then-
  jump; retires in-flight uops instead of discarding; byte-identical to the old
  code when the window is empty; drain bounded by the 1024-cycle horizon).
  OOO-only -- ALU/simple/in-order joins untouched (gate-verified).
- **Data impact.** OOO+MPI cells are now simulatable; the coremodel MPI family
  is re-run on 1.9.2 for single-binary consistency (v192 tag). Validation:
  the crashing cell completes at 3.92M cycles (OOO fastest on MPI bfs, as
  latency hiding predicts); non-bfs OOO cells reproduce v190 closely; OMP
  path unshifted. Note: thread-MPI bfs cells carry multi-percent
  run-to-run spread (worst case of the documented weave nondeterminism), so
  bfs census checks use a tolerance band, not bit-equality.

## 1.9.1 -- docs-only

Documentation refresh for the 1.8.3 -> 1.9.0 train (this file, plus co-sim MPI
window, measured MPI pricing, and OMP critical-path metric edits). No source
change; no data impact.

## 1.9.0 -- measured Garnet-fed-back pricing for thread-MPI (epoch-frozen)

- **Change.** Thread-MPI per-access latency now prices from **measured** Garnet
  congestion via epoch-frozen deterministic feedback; the analytical override
  is demoted to an escape hatch (`PIMID_MPI_ANALYTICAL_PRICING=1`). OMP is
  unchanged (rolling-EWMA live feedback).
- **Root cause of what it replaces.** The naive "read the live per-drain EWMA"
  feedback was 26-196% per-core / ~25% makespan nondeterministic (a fast rank
  folds fewer drains into the rolling scalar than a slow one) and read-depth
  inflated. Epoch-frozen table lookup (read epoch `k-1`'s frozen sample, keyed
  by the access's own ROI-relative cycle) removes the read-instant dependence.
  Three membership leaks fixed en route (pre-ROI pollution, startup-skew
  stamping, partial-bucket folds; DESIGN_190 section 10), built on the
  already-deterministic per-phase batch contents (defect #9 venue-independence).
- **Repeatability reality (residual).** Not bit-exact: the one-pass
  measured-feedback loop converges to a **host-dependent fixpoint** --
  within-host reproducible per-core, cross-host systematic; the rank-0 /
  cut-pinning core is host-independent. Fidelity:
  measured level +5-12% above the analytical floor.
- **Data impact.** Thread-MPI detailed sweeps priced with the old analytical
  override should be **regenerated** with measured feedback. MPI sweeps must be
  run **single-venue** for internal consistency (true venue-independent
  bit-exactness needs two-pass replay, future work). OMP and analytical-model
  cells unaffected.

## 1.8.8 -- OMP critical-path cycle summary (core-0-metric defect)

- **Defect.** Device-scope OMP runs had no critical-path aggregation. Both the
  sweep harness (`grep cycles | head -1`) and `parseZSimOutputFile()` latched
  onto the FIRST per-PE `cycles:` line = **core 0 only**, which reflects only
  that PE's active cycles, not kernel completion (the last PE to finish).
- **Root cause.** Core 0 was a low outlier at 16/64 PEs but representative at 32
  (16 PEs: core0 8% low; 64 PEs: 26% low; 32 PEs: representative), so recording
  it manufactured a spurious bfs HBM3 32-PE +28% cycle spike that "reversed" at
  64.
- **Fix.** Device-scope OMP now emits `OMP cycles: <max> (mean, min, pes,
  critical-path max)` plus a parser-compatible `Total: <sum> cycles (max: <max>)`
  (mirroring the MPI path). Purely additive; per-PE lines / `out.cycles` /
  power untouched.
- **Data impact.** Device-scope OMP sweeps parsed via `head -1` / core-0 carry
  the artifact and must be re-parsed against `(max: N)` (or re-run). The results
  harness OMP branch must switch from `grep cycles | head -1` to the `(max: N)`
  line (flagged out-of-scope in the commit).

## 1.8.7 -- co-sim MPI: tid/cid aliasing, zero-migration ranks, guard, protocolTail

- **Defect #15 class -- tid/cid ROI-baseline aliasing.** `roiRelCycles(tid)`
  read the ROI baseline table by **rank id** while it is written by **core
  index**. In system scope a rank's `cid != tid`, so `tid` 0..3 aliased HOST
  cores' ~16.5M pre-ROI (MPI_Init-era) clock; `roiRel` clamped to 0 and
  collapsed the SEND/RECV rendezvous (reduce root accumulated senders' stamps
  instead of advancing to their max). Fix: index by the rank's actual core
  (`cids[tid]`) + a per-rank last-valid-cid cache. Scoped to co-sim; device
  scope keeps `tid` indexing (correct and deterministic there).
- **Finalized model.** Ranks ARE the device PEs; the post-ROI collective tail
  (closing barrier + Reduce + Finalize) is device-resident, executed and priced
  ON THE PE. **Zero migrations** at window close (the 1.8.4/1.8.6 residual
  device->host legs, incl. rank 0's `roi_end` migration, all removed), which
  also eliminates the 1.8.6 exit race by construction.
- **Cross-axis invariant guard (defect-13 / 1.7.7 tripwire lineage).** At the
  recv rendezvous, a single message advance exceeding the ROI span means the
  clock axes are mismatched -- shout and cap rather than advance by a garbage
  delta. Eliminated a rare ~4% `2^32` rendezvous overflow (Garnet "event too
  far into the future").
- **protocolTail stat.** Per-PE receipt (final PE cycles minus closing-barrier
  marker); visibility only, never alters `cycles`. Co-sim-only, fixed global
  array, so device-scope `Core`/`initStats` stay byte-identical.
- **Data impact.** System-scope (co-sim) MPI data produced before 1.8.7 has the
  collapsed rendezvous and must be regenerated; the flaky ~4% overflow is gone.
  Device-scope MPI unaffected (reproduces the 1.8.6 distribution).

## 1.8.6 -- co-sim MPI exit-protocol heap corruption

- **Defect.** The 1.8.4 closing device->host migrate-out freed a rank's core
  slot mid-handler while a peer immediately entered its contention-sim phase
  pass; under the MPI serial weave the two overlapping handlers iterated
  glibc-heap state unsynchronized and **corrupted the process heap** (the
  "malloc(): unaligned tcache chunk" / QEMU SIGSEGV / Garnet "No output port for
  vnet" panics, and rarely the contention_sim "event too far into the future"
  assert, all right after the last rank drained).
- **Root cause (exit race).** Racy closing migrate-out under
  `simulation.parallel` serial weave (one vcpu may touch simulator state, but a
  migrate-out + peer phase pass overlap). The mirror migrate-IN at window open
  never corrupts.
- **Fix.** Drop the closing migrate-out; the drained rank finishes its short
  post-ROI protocol on its device PE (<1% accounting shift).
- **Data impact.** Pre-1.8.6 co-sim MPI runs crashed non-deterministically
  (~1/6 clean); no trustworthy pre-1.8.6 co-sim MPI data. gemv checksum
  unchanged from 1.8.4; histogram checksum 512.

## 1.8.5 -- reinstate per-rank flush + launch charges in the co-sim MPI window

- **Fix.** Reinstated the per-rank flush + launch boundary charges that 1.8.4
  had deferred, so every rank prices flush + launch on its host core at its
  kernel-entry barrier.
- **Data impact.** Negligible at sweep scale.

## 1.8.4 -- defect #14 co-sim MPI ROI window (+ 1.8.3 control-surface tidy)

- **Defect #14 (four layers deep).** Co-sim MPI measured dev=0. Root cause
  spanned: rank-0-only `roi_begin`; a `ROI_BEGIN` thread-branch early-return
  skipping the offload block; a parked-opener wall-order race making
  window-open-time migration impossible; and non-thread-safe
  migration/termination under the rank stampede.
- **Fix.** ROI is a device **window**: every rank charges + migrates ITSELF at
  its kernel-entry barrier (its own program order, no wall races), computes on
  its PE, migrates out at the closing barrier (later removed in 1.8.6/1.8.7);
  window close is a pure migration event. Migration bookkeeping serialized.
- **1.8.3 control surface.** Thread-based MPI rank emulation is the ONLY
  exec-method MPI model (`PIMID_MPI_THREADED` / `PIMID_MPI_PROCESS` deleted,
  legacy per-rank-process exec launcher removed -- per-rank processes remain
  only in the trace method); one knob `simulation.parallel` (default true)
  governs simulator parallelism for both APIs; MPI stays serial for
  determinism; `PIMID_EMIT_CONFIG_ONLY` honored in device scope.
- **Data impact.** Pre-1.8.4 co-sim MPI results measured a single rank / dev=0
  and must be regenerated. Known residuals at ship: deferred per-rank charges
  (fixed 1.8.5) and a nondeterministic post-measurement exit race (fixed 1.8.6).

## 1.8.2 -- defect #13: thread-MPI cycle rewind aborted every co-sim MPI run

- **Defect #13.** `ProcessStats::updateCore()` asserted a core's CYCLE counter
  is monotonic (`cCycles >= lastCoreCycles[cid]`). Under 1.6 thread-MPI it is
  not: a rank parking in a transport wait is rewound at `MPI_COMM_END`
  (`Core::pimidRewindCycles`) to erase wall-dependent wait growth and stay
  bit-exact, so the counter legitimately moves backwards -- the assert fired and
  QEMU took a SIGSEGV, killing every co-sim MPI cell (rc=1).
- **Root cause.** The cycle rewind (cycles-only; instruction count stays
  monotonic) collided with the cycle-monotonicity assert once a host domain and
  offload migration coexisted with real ranks -- first reachable only after
  1.8.0 stopped MPI running single-rank.
- **Fix.** Keep asserting INSTRUCTION monotonicity; mirror a cycle rollback in
  the process total instead of underflowing the unsigned subtraction.
- **Data impact.** Before 1.8.2 every co-sim MPI cell aborted (rc=1) with no
  output -- there is no pre-1.8.2 co-sim MPI data. This is the first release in
  which co-sim MPI ran correctly (16 ranks, no assert/SIGSEGV). Device scope
  unaffected (no host domain).

## 1.8.1 -- docs-only

Documentation for the 1.8.0 system-scope MPI + host-core fixes. No source
change; no data impact.

## 1.8.0 -- defect #12: system-scope MPI ran single-rank; host-baseline cores capped

- **Defect #12a -- host core count capped.** `host_num_cores` is parsed only
  from a top-level `host:` block, but system-scope configs declare the host
  under `system.hosts[]`, so it stayed at its default (4) -- capping OMP
  threads, MPI ranks, and the devorg `--pes` injection regardless of the
  configured count. Fix: sync it from the parsed HOST system node.
- **Defect #12b -- system-scope thread-MPI never wired.** The `libpimid_mpi.so`
  LD_PRELOAD plus `PIMID_MPI_RANKS`/`PIMID_MPI_THREADED` existed only in the
  device-scope branch, so every system-scope MPI run (host baselines AND co-sim)
  resolved MPI against the system runtime and executed as a SINGLE rank
  (rank 0, size 1) silently at exit 0. Fix: wire thread-MPI in system scope and
  force `parallelism=1` there (ranks run by zsim's deterministic round-robin
  core rotation, which never engages at parallelism>1).
- **Data impact.** System-scope MPI results produced before 1.8.0 are
  single-rank (and core-count-capped) and must be regenerated. Device-scope
  sweeps were never affected (device launch path; all edits gate on
  `scope == "system"`).
