#!/bin/bash
# PIMID docker entrypoint: greet user, then exec whatever they passed
# (default: bash).
cat <<'EOF'
╔══════════════════════════════════════════════════════════════════════════════╗
║   PIMID — Full-System Simulator with Intricacy and Diversity for PIM         ║
║   docker image: ghcr.io/isaacyhe/pimid                                       ║
╚══════════════════════════════════════════════════════════════════════════════╝

Quick start:
  pimid --help                                     # CLI help
  pimid_smoke                                       # run feature sweep
  pimid --method exec --config examples/tech_DDR4.yaml --no-power

Configs:   /opt/pimid/examples/         (11 mem techs, 5 cores, 8 NoC topos, 2 net models)
Cosim:     /opt/pimid/examples/cosim/   (host+device YAMLs)
Workloads: /opt/pimid/benchmarks/       (built PIM kernels, classic, NPB)
Cosim wl:  /opt/pimid/benchmarks/cosim/ (built cosim kernels)
Source:    https://github.com/isaacyhe/pimid

EOF
exec "$@"
