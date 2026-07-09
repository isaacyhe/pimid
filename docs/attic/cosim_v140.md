# [RETIRED 1.7.5] Host-Device Co-simulation (v140 design)

> **This document describes the pre-1.7 co-simulation model and is retired.**
> The shipped 1.7.0-1.7.4 co-sim system replaced the flat host<->device
> "interconnect link" charge with a two-layer bridge (protocol x phy), added
> Case-1/Case-2 coherence, a kernel-launch cost tree, an analytic host
> crossbar fabric, and the `is_default_mem` / `host.mem` memory-topology knob.
> See the live [../cosim.md](../cosim.md) and
> [../cosim_calibration.md](../cosim_calibration.md). The `system.network.links[].type`
> table below (interposer / cxl / pcie / nvlink presets) is the retired flat
> charge; it is still parsed for backward compatibility but is superseded by
> `system.bridge.*` whenever a system-scope co-sim config is emitted.

---

Co-simulation (`scope: system`) composes the two models the simulator
already has -- the host is the host, the device is the device -- and
models only the interactions between them:

- Out-of-ROI code executes on the host model (OoO/in-order cores, the cache
  hierarchy, host memory through its own controller).
- The ROI region executes on the device model: PEs behind the device's own
  NoC in front of the device's own memory technology -- the identical code
  path a standalone `scope: device` simulation uses.
- Boundary crossings are charged with the link + memory technology models. A
  device with `attachment: internal` is the host's main memory (no outside
  transfer); any other attachment pays the configured link.

## Interconnect link types (`system.network.links[].type`) -- RETIRED

| type | base latency | bandwidth | models |
|---|---|---|---|
| `interposer` | ~5 ns | 256 GB/s | 2.5D on-package (HBM-on-interposer) |
| `cxl_3_0` / `cxl_2_0` | 100 / 200 ns | 126 / 63 GB/s | coherent expansion |
| `pcie_gen5` / `pcie_gen4` | 500 ns | 63 / 31.5 GB/s | discrete accelerator card |
| `nvlink_3_0` / `nvlink_4_0` / `nvlink_c2c` | 100-700 ns | 50-450 GB/s | proprietary fabrics |
| `ualink_1_0` | ~100 ns | ~450 GB/s | accelerator fabric |

This flat per-link charge is superseded by the two-layer bridge
(`system.bridge.protocol` x `system.bridge.phy`); see the live cosim.md.
