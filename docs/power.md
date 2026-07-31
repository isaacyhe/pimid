# Power and Area

Every number here comes from a tool, driven by measured activity. Where something
is assumed rather than measured, this page says so — the assumptions matter as
much as the results.

## What prices what

| component | timing | power | area |
|---|---|---|---|
| processing elements | zsim | McPAT | McPAT |
| element memory interfaces | queueing model | McPAT | McPAT |
| memory array (DRAM) | Ramulator2 | Ramulator2 | CACTI |
| memory array (NVM) | NVSim | NVSim | NVSim |
| caches | zsim | McPAT | CACTI |
| interconnect | Garnet | McPAT | McPAT |
| memory controllers | — | McPAT | McPAT |

Power analysis is on by default; `--no-power` disables it for fast timing sweeps.
`pim.pe.energy_factor` scales reported per-operation element energy and does not
affect timing.

## The reference class

A power model prices a die against a measured population, and that choice matters
more than any single parameter. McPAT offers two: server processors, where the
undifferentiated-core term is a curve fitted to Niagara, Merom, Penryn and
Opteron die measurements; and embedded parts, calibrated against ARM designs and
a parametrized-processor study.

**Device scope selects the embedded population. Host scope selects the server
one.** A processing element on a memory die is not a fragment of a server
processor, and pricing it as one made the undifferentiated term — which carries
no dynamic power at all, only leakage — larger than everything the description
actually named.

This is not a discount. It is the other of the two populations the tool was
calibrated against, and the one a memory-die element belongs to.

**It does not claim in-memory logic is cheap.** The literature is consistent that
logic built in a memory process is slower and larger than the same logic in a
logic process. That penalty is a separate, still-missing term: elements are
priced in a logic process today while a memory-process clock is already assumed
for them. The two halves disagree about which fab built the element, and the
correction runs opposite to the reference-class one.

## The element

The element is composed from what it is, rather than borrowed from a processor
with fields turned down. Its arithmetic follows the datapath — one unit of each
kind per lane, since a lane that cannot multiply or cannot do floating point
stalls on kernels for which the timing model charges no stall. Its instruction
store is an explicitly sized resident memory. Its datapath width is the same
field the timing model charges through, not a second one.

The knobs are `pim.pe.lanes`, `pim.pe.operand_width`, `pim.pe.floating_point` and
`pim.pe.imem_bytes` — see the configuration reference, which also records where a
knob reaches only one half of the model and warns rather than pretending
otherwise.

Residual, stated rather than hidden: the instruction store is still built by a
cache constructor, so it carries a tag array and single-entry miss, fill and
prefetch structures a scratchpad does not have. That overstates it.

## The memory array

**There is one memory, and it is charged once.** This build models one host and
one memory: the memory either *is* the processing device, or it is a plain
non-PIM main memory and the run is host-only. In a co-simulation the host's
accesses and the elements' accesses land on the same silicon, so the charge is
their sum, applied once. A host term beside a device term would price that
silicon twice.

If two nodes ever name different memory technologies the run stops, rather than
charging one and dropping the other — that is a topology this build does not
model, and mispricing it silently would be worse than refusing.

Non-DRAM technologies are not charged in system scope yet, and say so rather than
printing a number shaped like a DRAM one.

## Measured, and assumed

Measured from the run: element and host activity counts, cache accesses and
misses, interconnect packets and hops, memory reads and writes, cycles.

**Assumed — worth knowing before quoting a result:**

- **The instruction mix.** Nothing counts floating-point, multiply or integer
  instructions; the emulator knows the opcode and the plugin discards it before
  the timing model sees it. The power model splits non-branch instructions by a
  fixed ratio and drives the execution bypass with fixed fractions. A
  memory-bound kernel and a compute-bound one therefore receive the same mix.
- **Interface energy** per access is an inherited constant, not a measurement.
- **Power gating** has no temporal weighting of its own.

## Reproducibility

Given a deterministic instruction stream the simulator is exact: repeated runs of
a single-threaded workload produce bit-identical cycles and access counts.

Parallel workloads vary between runs. That variation belongs to the workload, not
the simulator — the host kernel schedules the guest's threads and the emulator
reflects that faithfully, exactly as a parallel program on real hardware does not
repeat its interleaving. Total work stays stable; simulated time moves.

Two things follow. Differences smaller than that variation are not findings. And
any regression can be gated against a single-threaded build and required to be
bit-exact, which is a sharper check than judging a parallel run through its own
noise.
