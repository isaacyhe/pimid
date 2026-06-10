# Network Models

PIMID has exactly two NoC models (`noc.model`):

## `detailed` — cycle-accurate Garnet (the default)

The full Garnet 3.0 engine (extracted from gem5): routers, virtual channels,
credit-based flow control, deadlock-free routing.

- **DRAM device networks are trees.** A DRAM device's internal datapath is a
  hierarchical distribution fabric, not a mesh — PIMID emits a per-technology
  CUSTOM tree topology (banks -> channel-DQ -> system root) with per-link
  latency/width derived from the technology's JEDEC organization (per-channel
  bandwidth x channel count from Ramulator2). Per-technology bandwidth is
  modeled by NI-side flit count; tree routing uses up/down virtual-channel
  classes and is deadlock-free (validated drain-complete across packet counts,
  technologies, and traffic patterns).
- Flat topologies remain available for the host network and non-DRAM device
  memories: `MESH_2D`, `TORUS_2D`, `RING`, `CROSSBAR`, `FAT_TREE`, `BUS`,
  `H_TREE`, `CUSTOM` (file-defined).

## `analytical` — closed form

Per-access effective latency:

```
t_eff = max( (L + W_q) / M ,  P * D / c )
```

- `L` — unloaded latency: hop-count round trip + serialization + memory access.
  For DRAM the per-hop link latency derives from the technology's
  **per-channel** bandwidth (the DQ datapath one access actually traverses),
  hop-normalized to the per-channel subtree depth; aggregate multi-channel
  bandwidth enters through the floor term instead.
- `W_q` — M/D/1 queuing contention at the bottleneck channel, computed from
  the aggregate injection rate spread over the topology's channels
- `M` — memory-level-parallelism intensity (`noc.mlp`; omit for the
  calibrated per-core-model default)
- `P * D / c` — aggregate-bandwidth floor: P PEs sharing c channels of
  deterministic service time D

`M = 1` degenerates to a fully serial latency model; large `M` approaches the
bandwidth roofline. The default `M` is calibrated against `detailed`.
Runs at analytical speed (~seconds) — use it for large design-space sweeps,
and `detailed` for ground truth.

## Synthetic traffic mode

`--method synthetic` injects parametric traffic directly into Garnet — no
workload or QEMU needed:

```yaml
synthetic:
  pattern: uniform        # uniform, bit-complement, tornado, neighbor,
                          # transpose, bit-reverse, bit-rotation, shuffle,
                          # memory-directed (PE -> memory-node hotspot)
  injection_rate: 0.1     # or injection_rate_min/max/step for a sweep
  packets: 10000
  warmup: 1000
```

Reports total cycles, average latency, and throughput per injection rate.

## Per-level and bridge overrides

When PEs sit deep inside a DRAM device (`placement: BANK` or `SUBARRAY`),
memory traffic does not cross one flat network — it climbs a hierarchy:

```
subarray -> bank -> bank_group -> chip -> rank -> channel -> system
```

Each rung is physically different silicon: a subarray's local wiring, the
bank I/O, the shared channel DQ bus. Real DRAM datapaths are asymmetric —
wide and parallel near the arrays, narrow and shared toward the channel.
PIMID exposes that structure through two override mechanisms. If you never
touch them, the per-technology JEDEC-derived defaults apply; they exist for
expert tuning and design-space exploration.

### Level overrides (`noc.levels.<level>`)

Give any hierarchy level its own network parameters instead of the global
`noc.*` settings. Valid level names: `subarray`, `bank`, `bank_group`,
`chip`, `rank`, `channel`, `system`.

```yaml
noc:
  model: detailed            # global default
  levels:
    bank:
      model: simple          # this level: per-level analytical path
      link_width_bits: 64    # narrow bank I/O
      frequency_ghz: 2.4
    chip:
      model: detailed        # this level: full Garnet
      router_latency: 3
      virtual_channels_per_vn: 8
```

Per-level knobs include `model`, `topology`, `link_width_bits`,
`frequency_ghz`, `latency_cycles`, `router_latency`, `router_pipeline`,
`router_bypass`, `virtual_networks`, `virtual_channels_per_vn`, and
input/output buffer depths.

### Bridge overrides (`noc.bridges.<boundary>`)

The boundaries BETWEEN adjacent levels are modeled as bridges — the
mux/TSV/bus transition points where the datapath changes width and clock.
Valid boundary names: `subarray_bank`, `bank_bankgroup`, `bankgroup_chip`,
`chip_rank`, `rank_channel`, `channel_system`.

```yaml
noc:
  bridges:
    bank_bankgroup:
      lower_width_bits: 64   # bank side of the crossing
      upper_width_bits: 8    # serialized toward the bank group
      latency_ns: 1.0
    chip_rank:
      model: detailed
      router_latency: 2
```

A bridge override is the natural place to model serialization chokepoints
(e.g. a wide internal prefetch funneling into a narrow external bus) without
touching either adjacent level.

### When to use which

- **Sweep a single bottleneck** — override one bridge (e.g. shrink
  `bank_bankgroup` width) and watch the bandwidth knee move.
- **Mixed fidelity** — keep the contended level on `detailed` Garnet and let
  quiet levels run the cheap per-level analytical path.
- **What-if datapaths** — give a level a non-default topology or clock to
  model a hypothetical device organization.

The complete key list with defaults is in
[yaml_reference.md](yaml_reference.md) (sections "Per-Level Overrides" and
"Bridge Overrides").
