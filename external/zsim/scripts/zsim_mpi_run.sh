#!/bin/bash
# Run MPI workloads under ZSim - wrapper for mpirun
# Usage: zsim_mpi_run.sh -np <ranks> [-cfg <template.cfg>] [-o <outdir>] -- <mpi_binary> [args]
#
# Example:
#   ./scripts/zsim_mpi_run.sh -np 4 -- /tmp/mpi_saxpy
#   ./scripts/zsim_mpi_run.sh -np 4 -cfg configs/mpi_template.cfg -o /tmp/results -- ./my_mpi_app

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ZSIM_DIR="$(dirname "$SCRIPT_DIR")"

# Defaults
NP=4
TEMPLATE_CFG="$ZSIM_DIR/configs/mpi_template.cfg"
OUTDIR=""
MPICH_DIR="/opt/mpich"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -np) NP="$2"; shift 2 ;;
        -cfg) TEMPLATE_CFG="$2"; shift 2 ;;
        -o|--output) OUTDIR="$2"; shift 2 ;;
        --mpich) MPICH_DIR="$2"; shift 2 ;;
        --) shift; break ;;
        -h|--help)
            echo "Usage: $0 -np <ranks> [-cfg <template.cfg>] [-o <outdir>] -- <binary> [args]"
            echo ""
            echo "Options:"
            echo "  -np N         Number of MPI ranks (default: 4)"
            echo "  -cfg FILE     ZSim config template (default: configs/mpi_template.cfg)"
            echo "  -o DIR        Output directory (default: /tmp/zsim_mpi_<pid>)"
            echo "  --mpich DIR   MPICH installation directory (default: /opt/mpich)"
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

BINARY="$@"
if [ -z "$BINARY" ]; then
    echo "ERROR: No binary specified"
    echo "Usage: $0 -np <ranks> [-cfg <template.cfg>] [-o <outdir>] -- <binary> [args]"
    exit 1
fi

# Check requirements
if [ -z "$PINPATH" ]; then
    echo "ERROR: PINPATH environment variable not set"
    echo "  export PINPATH=/path/to/pin"
    exit 1
fi

if [ ! -x "$MPICH_DIR/bin/mpirun" ]; then
    echo "ERROR: MPICH not found at $MPICH_DIR"
    echo "  Build MPICH with: --with-device=ch3:nemesis --prefix=$MPICH_DIR"
    exit 1
fi

# Set output directory
if [ -z "$OUTDIR" ]; then
    OUTDIR="/tmp/zsim_mpi_$$"
fi
mkdir -p "$OUTDIR"

echo "========================================"
echo "ZSim MPI Runner"
echo "========================================"
echo "Ranks:    $NP"
echo "Binary:   $BINARY"
echo "Output:   $OUTDIR"
echo "Template: $TEMPLATE_CFG"
echo "========================================"

# Create per-rank wrapper that ZSim will run
WRAPPER="$OUTDIR/rank_wrapper.sh"
cat > "$WRAPPER" << 'WRAPPER_EOF'
#!/bin/bash
RANK=${PMI_RANK:-0}
OUTDIR_BASE="$1"
shift
BINARY="$@"

RANK_DIR="${OUTDIR_BASE}/rank${RANK}"
mkdir -p "$RANK_DIR"

# Copy config to rank directory
cp "${OUTDIR_BASE}/zsim_template.cfg" "${RANK_DIR}/zsim.cfg"
echo "process0 = { command = \"$BINARY\"; };" >> "${RANK_DIR}/zsim.cfg"

cd "$RANK_DIR"
export ZSIM_FORCE_POSIX_SHM=1
exec "${ZSIM_DIR}/build/opt/zsim" zsim.cfg 2>&1 | tee zsim.log
WRAPPER_EOF
chmod +x "$WRAPPER"

# Copy template config (remove process0 line if exists)
grep -v "^process0" "$TEMPLATE_CFG" > "$OUTDIR/zsim_template.cfg" 2>/dev/null || cp "$TEMPLATE_CFG" "$OUTDIR/zsim_template.cfg"

# Export for wrapper
export ZSIM_DIR
export PINPATH

# Run MPI with wrapper
echo ""
echo "Starting $NP MPI ranks under ZSim..."
"$MPICH_DIR/bin/mpirun" -np "$NP" "$WRAPPER" "$OUTDIR" $BINARY

echo ""
echo "========================================"
echo "Results Summary"
echo "========================================"

# Collect results
for r in $(seq 0 $((NP-1))); do
    RANK_DIR="$OUTDIR/rank${r}"
    if [ -f "$RANK_DIR/zsim.out" ]; then
        cycles=$(grep "cycles:" "$RANK_DIR/zsim.out" | head -1 | awk '{print $2}')
        instrs=$(grep "instrs:" "$RANK_DIR/zsim.out" | head -1 | awk '{print $2}')
        echo "Rank $r: cycles=$cycles instrs=$instrs"
    else
        echo "Rank $r: NO OUTPUT"
    fi
done

echo ""
echo "Full results in: $OUTDIR/rank*/zsim.out"
