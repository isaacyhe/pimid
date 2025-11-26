# PIMID Documentation Index

> **Start Here**: New to PIMID? Read the [Quick Start Guide](../QUICKSTART.md) first!

This directory contains all PIMID documentation organized by purpose.

---

## For Users

| Document | Description |
|----------|-------------|
| [Quick Start Guide](../QUICKSTART.md) | 5-minute setup and first simulation |
| [Configuration Guide](CONFIGURATION_GUIDE.md) | YAML config file reference |
| [Unified PIMID Usage](UNIFIED_PIMID_USAGE.md) | Command-line interface and options |
| [Execution Models](EXECUTION_MODEL_CONFIGURATION.md) | ZSim, event-driven, analytical modes |

---

## Architecture & Design

| Document | Description |
|----------|-------------|
| [Architecture Overview](ARCHITECTURE.md) | System architecture and components |
| [External Model Integration](external_model_integration.md) | ZSim, Ramulator, GARNET integration |
| [GARNET Integration](GARNET_INTEGRATION.md) | Network-on-chip simulation details |
| [Hierarchical Power Modeling](HIERARCHICAL_POWER_MODELING.md) | McPAT/CACTI power model integration |

---

## External Simulators

### ZSim (Instruction-Level Simulation)
| Core Type | Description |
|-----------|-------------|
| **Simple** | Single-issue in-order core with cache |
| **OOO** | Out-of-order superscalar (4-wide, 128 ROB) |
| **ALU** | Cacheless ALU-only core for PIM |
| **Timing** | Timing core with cache hierarchy |
| **Null** | Fast-forward core (no timing) |

See: [ZSim Event-Driven Implementation](ZSIM_EVENT_DRIVEN_IMPLEMENTATION.md)

### Ramulator (DRAM Timing)
| DRAM Type | Description |
|-----------|-------------|
| **DDR3** | DDR3-1600K, 2Gb x8 |
| **DDR4** | DDR4-2400R, 8Gb x8 |
| **DDR5** | DDR5-4800, 16Gb x8 |
| **HBM2** | High Bandwidth Memory 2.0 |
| **HBM3** | High Bandwidth Memory 3.0 |

### GARNET (Network-on-Chip)
| Topology | Description |
|----------|-------------|
| **MESH_2D** | 2D mesh with XY routing |
| **TORUS_2D** | 2D torus (wraparound) |
| **FAT_TREE** | Fat-tree for HPC |
| **DRAGONFLY** | High-radix dragonfly |
| **CROSSBAR** | Full crossbar |
| **H_TREE** | Hierarchical tree (baseline) |

See: [Network Topology Config](config_schema_network_topology.md)

---

## Testing & Validation

| Document | Description |
|----------|-------------|
| [Test Summary](TEST_SUMMARY.md) | Overall test results |
| [Comprehensive Test Report](COMPREHENSIVE_TEST_REPORT.md) | Detailed test analysis |
| [Code Audit Report](CODE_AUDIT_REPORT.md) | Code quality audit |

### Test Suites

| Suite | Tests | Pass Rate | Command |
|-------|-------|-----------|---------|
| Quick Smoke | 5 | 100% | `python3 test/scripts/test_quick_execution_models.py` |
| Comprehensive | 1,000 | 100% | `python3 test/scripts/comprehensive_test_suite_zsim_1000.py` |
| **ZSim External Models** | **1,000** | **100%** | `python3 test/zsim_external_model_test_suite_1000.py` |
| Production Audit | 10,000 | 100% | `python3 test/scripts/comprehensive_audit_suite_zsim_10000.py` |

---

## Development

| Document | Description |
|----------|-------------|
| [Plugin Development Guide](PLUGIN_DEVELOPMENT_GUIDE.md) | How to create PIMID plugins |
| [Project Status](PROJECT_STATUS.md) | Current status and roadmap |
| [Submodule Notes](SUBMODULE_NOTES.md) | Git submodule management |

---

## Research (DAC'26)

| Document | Description |
|----------|-------------|
| [DAC'26 README](../DAC26/README.md) | LIBCom vs H-tree evaluation |
| [DAC'26 Quick Start](../DAC26/QUICKSTART.md) | Running DAC'26 experiments |
| [Workloads Summary](../test/benchmarks/WORKLOADS_SUMMARY.md) | All 16 benchmark workloads |

---

## Implementation Reports (Archive)

Detailed reports from development phases are in [`implementation_reports/`](implementation_reports/):

- ALU Core PIM Verification
- Bank/Subarray-Level PIM Comparisons
- GARNET Implementation Report
- Inner-Bank Timing Analysis
- Integration Fixes Summary

---

## Directory Structure

```
docs/
├── README.md                      # This index
├── ARCHITECTURE.md                # System architecture
├── CONFIGURATION_GUIDE.md         # Config file reference
├── EXECUTION_MODEL_CONFIGURATION.md
├── external_model_integration.md  # ZSim/Ramulator/GARNET
├── GARNET_INTEGRATION.md
├── HIERARCHICAL_POWER_MODELING.md
├── PLUGIN_DEVELOPMENT_GUIDE.md
├── UNIFIED_PIMID_USAGE.md
├── ZSIM_EVENT_DRIVEN_IMPLEMENTATION.md
├── implementation_reports/        # Archive of dev reports
└── presentations/                 # Conference slides/papers
```

---

## Quick Reference

### Memory Technologies
- **SRAM**: Fast, volatile (via CACTI)
- **DRAM**: Standard memory (via Ramulator)
- **STT-MRAM**: Non-volatile, fast read (via NVSim)
- **PCM**: High density, slow write
- **ReRAM**: Non-volatile, good endurance

### PIM Granularities
- **SUBARRAY**: Fine-grained, lowest latency
- **BANK**: Balanced performance/area
- **CHIP**: Coarse-grained
- **RANK**: System-level

### Workloads (16 total)
**Message Passing**: bfs, gemm, spmv, dotproduct, reduction, histogram, prefixsum, stencil1d
**Shared Memory**: bfs, gemm, spmv, dotproduct, reduction, histogram, prefixsum, stencil1d

---

**Last Updated**: 2025-11-26
