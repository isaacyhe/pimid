# Changelog / defect ledger

Release + defect ledger for the co-sim MPI window, the measured-feedback MPI
pricing model, and the OMP critical-path metric. One entry per release; each
gives the defect fixed, a one-line root cause, and a data-impact note (which
sweep generations the fix invalidates or corrects). Authoritative source is the
release commit messages; deeper design rationale for 1.9.0 is in
`docs-dev/DESIGN_190_PDES.md`.

## 1.11.16 -- the audit's fixes are audited, and three of them fall

Seven adversarial verifiers (one per 1.11.15 fix claim + a fresh sweep of the
hotfix diff) REFUTED three clusters. The repairs, all in this release:

POWER GATING HAD NO REAL ENDPOINTS. CACTI never assigns
power_gated_leakage anywhere in the vendored fork -- the 1.11.15 enable
repair opened a path to a value nothing writes. For set-associative arrays
(every cache data/tag array) McPAT multiplied that never-written zero, so
the gated endpoint was 0 and leak_eff credited ~100% leakage elimination
-- ANTI-conservative, the opposite of the in-code caveat. Fixed in the
tool: array.cc now computes the analytic Vcc_min/Vdd retention endpoint
for ALL array classes, the same model McPAT's own logic/NoC/MC units
already use. The residual is ~0.35 of active subthreshold, not ~0.

THE GATED ENDPOINT KEPT A DISCOUNT THE ACTIVE ONE DROPPED. applyFam
rebases active leakage on the plain value (the comm-dram ratio already
encodes a long-channel device) but only SCALED the gated fields, so the
default long-channel read compared mismatched bases and over-credited PG
savings ~2.25x. The recorded "15% sleep-tx residual" was
0.35 x long-channel-factor -- an artifact, not an endpoint. Fixed: gated
fields rebase exactly like lines 435-436. applyPG also now interpolates
SUBTHRESHOLD only (a sleep rail does not remove gate-oxide tunnelling),
warns two-sidedly (near-zero endpoints too, on stderr -- the child's
stdout is /dev/null), and scales the per-level NoC breakdown with the
aggregate.

THE CENSUS REJECT PATH EMITTED THE 1.9.28 DEGENERACY. The >5% reject set
mi=mf=0 and "fell through" into the assignment below it -- McPAT was told
0-int/0-fp/100%-branch, strictly worse than the fractions it claimed to
restore. The residual-as-int correction sat inside the print-once latch,
so the number depended on whether a message had printed (and the forked
child's regenerated XML differed from the parent's). Restructured:
census_ok flag, value logic decoupled from printing, fractions recomputed
on the census-branch base so classes always sum to retirement, and a
rejected census reverts its branch class too.

INJECTED CHARGES NOW SUBTRACT ON EVERY CORE TYPE. Barrier/PCIe/drain
charges are cycles-as-instrs; only OOOCore reported syntheticInstrs (by
oooBbl absence, which also misclassified real >1024-insn fallback TBs).
BblInfo carries an explicit synth flag set at the eight injection sites;
the shared mix machinery counts and reports it for alu/simple/null/
in-order/OOO alike, so MPI cells no longer manufacture a census deficit.

Also: MCPHY gated on PLACEMENT alone (SRAM/NVM on-die element MCs lose
the off-chip PHY they never had); per-node pg: keys parse in system scope
(devices[].pim.pe.pg / .pim.mc.pg / .noc.pg) with residencies printed per
node; PE-MI locality line now prints in device scope (where the placement
corpus lives); coherence-flush writebacks use the configured line size and
the "charged" line prints only where a branch lands the charge;
pj_per_bit_override actually overrides; crossing bytes priced on the
first host only, with unpriced links marked in the summary; NullCore
rebases its census at ROI; in-order deferred path counts mix/FP for the
block it simulates; CACTI sleep-tx pointers initialized, inverted
destructor conditions fixed, degenerate-width wakeup no longer +inf, and
the softened compute_gate_area assert says so once on stderr; family
factors deduplicated behind one helper; stale pre-1.11.12 wrapper copy
(src/mcpat_wrapper.*, not in the build) moved to attic/.

Data impact: any 1.11.8-1.11.15 run with pg: true is invalid (PG savings
over-credited); PG-off runs are unaffected. SRAM/STT-MRAM/PCM/ReRAM cells
at subarray..chip placement re-price (off-chip MCPHY removed) -- area and
MC power drop. MPI/co-sim cells re-price their instruction base
(syntheticInstrs now subtracted on ALU PEs). Corpus re-sim (already
gated on this train) covers all three.

## 1.11.15 -- the train audits itself, and fixes what it finds

A 30-agent, 5-round adversarial audit of everything 1.11.2-1.11.14 shipped
returned 200 findings (14 blockers). The blockers, all fixed here:

POWER GATING WAS STILL DEAD END TO END. CACTI's error_checking()
unconditionally overwrote the power_gating enable with five sub-flags
nothing ever sets, so the enable emitted since 1.11.8 never survived into
any array: every gated endpoint was zero (an ideal 100%-savings model on
caches), and on family components the logic-priced gated value exceeded
the family-priced active one, so the clamp silently pinned savings to
ZERO. The 1118 gate misread the byte-identical areas as "area model does
not engage" -- they were proof the enable never arrived. Fixed: the flag
is honored (OR, not overwrite); applyFam scales the gated endpoints with
the same family transform; the clamp prints the inversion it used to
hide; setPGSpec now also fires in system scope.

THE MIX CENSUS WAS DEAD ON THE DEFAULT ELEMENT. Decode was gated on an
OOO/InOrder core existing, so alu_core -- the corpus default -- never
filled the census: the counted mix silently reverted to fractions and the
FP-without-FPU report could never fire, on exactly the cells 1.11.10 and
1.11.11 were built for. Decode is now always on (PIMID_NODECODE escape
hatch); ALU timing is untouched (it consumes instrs and bytes, which the
decoded block carries identically). The census also gains its BRANCH
class end-to-end, replacing the conditional-only core counter, and the
consistency check becomes symmetric: a >5% classified deficit rejects the
measurement loudly, a smaller one is priced as integer and printed --
nothing is silently charged zero execution energy any more.

CROSSING ENERGY OVER-CHARGED THREE WAYS. The coherence flush -- 99.999%
of counted bytes -- is a host writeback to MEMORY and is now charged as
line writebacks on the host-visible array, not as PCIe traffic. The link
is priced ONCE (host end carries the bytes; the device end keeps its
controller with zero transfer) instead of once per endpoint. And McPAT's
MCPHY -- a per-bit off-chip I/O driver on the same accesses the
termination term prices -- is no longer built for on-die element MCs,
which drive no DQ pins (type=1 embedded, no PHY).

THE LINK VOCABULARY NO LONGER KILLS FINISHED RUNS. cxl_2_0/cxl_3_0,
nvlink_3_0/4_0/c2c and interposer now resolve by family; unknown types
warn and price the transfer at zero instead of exiting after the
simulation completed; the override sentinel accepts 0.0 (an interposer
user can say "free").

Also: fam_core_power_ratio_ lost its assignment in 1.11.12, so the
CoreBreakdown printed logic-priced blocks under a family-priced total --
the blocks are now transformed directly (dynamic x fd, leakage x fl).
And the PE-MI locality counters were parsed at ROOT scope while zsim
emits them under pe-mi (MEM scope): always zero, report never printed.
Moved; a run with 698,189 remote accesses on disk now reports them.

Gate 1125 (mi100): ALU timing bit-equal under decode-always (10,164,688
both); census alive on the ALU cell (mixInt 172,842 where 1.11.14
measured zero); PG leakage 5.8e-6 -> 1.2e-6 W at 93% idle, and the
arithmetic back-solves to a gated endpoint of 8.7e-7 W -- a 15%
sleep-transistor residual, i.e. a REAL tool-computed endpoint where the
audit proved a 0% ideal; sleep-tx area now engages; no inversion
warnings; repeats to the digit. Two more never-executed-code faults were
flushed out by the resurrected enable: CACTI's Sleep_tx asserted on
degenerate cells (the 1-entry pure-RAM istore is too narrow to fold a
transistor into) -- both sites now degrade to a zero-area sleep network
in CACTI's own early-return style instead of killing the McPAT child.

DATA IMPACT: broad and intended -- PG becomes real, the counted mix
reaches the default element, crossing energy stops triple-counting.
The remaining 101 defects and 34 risks are triaged in
_1115audit/DIGEST.md (local) for the checkpoint discussion.

## 1.11.14 -- the calibration moves into the tool it calibrates

The border cleanup. The JEDEC k-calibration (1.11.1) was computed by the
caller: PIMID held the vendor-density table, the generation map and the
arithmetic, and applied them to CACTI's output at two separate call sites.
That is model logic in the orchestrator, which the borders rule forbids and
which had already produced one duplicated code path (1.11.9's system-scope
helper). Density, generation map and calibration now live inside the CACTI
fork; both call sites describe the array and ask
CACTIWrapper::getCalibratedDieArea().

The scope gate is the substance, not the packaging. McPAT links this SAME
cacti7 library and issues thousands of cache, register-file and TLB queries
through it -- a DRAM vendor-density factor reaching those would be a
category error with no symptom until someone checked an L2 area. So
calibration requires BOTH a commodity-DRAM main-memory query AND a named
technology; every other query returns raw CACTI, byte-identical to before.
The gate proves it with an SRAM cell rather than asserting it.

Two runs still happen for a calibrated die, and that is the model rather
than an artefact: k is defined against the PRESET organisation while the
reported die is the EFFECTIVE one, so reconfiguring banks moves the area by
CACTI's structural derivative around a vendor-anchored point.

Gate 1124 (mi100): HBM3 die line character-identical (40.78 mm^2/die,
k=0.054, raw CACTI 755.76) and DDR5 likewise (13.90, k=0.538) -- the
migration is numerically inert across technologies with very different k.
The leak arm: non-DRAM area identical (13 mm^2 both) and a DETERMINISTIC
1-PE SRAM cell identical in power and area (0.4 W, 7.1 mm^2). The
16-PE SRAM cell's power is not reproducible across jobs for the SAME
binary -- 3.2 W in gate 1123, 3.3 W here, unchanged 1.11.13 both times --
so that arm now asserts area identity plus a deterministic power cell,
which is what can actually be asserted.

DATA IMPACT: none. Same model, same numbers, computed where it belongs.

## 1.11.13 -- corners exist where the tables have them, and nowhere else

#121 asked for a per-technology corner axis: a speed corner for GDDR6 and
HBM, a mobile corner for LPDDR5 and the NVMs, derived from CACTI's own
device columns and activated where the 1.9.10 IDD data showed one corner
failing. Reading the tables answered the design question before the IDD
check could: each CACTI table carries exactly ONE commodity-DRAM device
column. lp-dram, the only alternative, is all-zero at 22 nm. A speed
corner for HBM periphery cannot be derived from a table that does not
contain one, so it is not derived -- the request is refused there, with
the reason printed, rather than approximated into existence.

Where corners DO exist is the logic family: CACTI carries hp, lstp and
lop, and McPAT already selects between them through sys.device_type. That
choice was simply never exposed, so every logic domain in every run so far
was priced hp without saying so. power.device_corner now exposes it
(hp/lstp/lop, validated, default hp = every existing run bit-identical),
which is what a host or a base-die design actually needs.

The area-factor uncertainty band is printed beside the factor in use:
2.44x is the linear l_phy ratio, the conservative end of a band whose
other end is its square (5.95x), with the UPMEM die as the only silicon
anchor between them. A reader can now see the width of the claim without
reading the source.

DATA IMPACT: none at defaults. Selecting a non-hp corner changes
logic-family components only.

## 1.11.12 -- the DRAM-periphery family becomes a property of the machine

#120 and the model-borders migration together, because they are the same
change. Since 1.11.2 the periphery transform lived in the wrapper, scaling
McPAT's CORE result after the fact -- which meant the on-die fabric and the
element controllers, silicon on the same die as the PEs, kept being priced
in a logic process, and the transform sat outside the tool that owns the
components. Both are now one thing: McPAT's XML carries a DRAM_PERIPHERY
device family (dram_periph_family/area/dyn/leak/scope), and McPAT applies
it internally to the components the scope names -- PE cores, their caches,
the on-die NoC and the element controllers -- then rebuilds its own totals
from the transformed parts. The wrapper describes the family and reads
whatever McPAT produced; it no longer post-scales anything.

Two consequences beyond tidiness. The aggregate and the parts can no longer
drift apart, which is the failure 1.11.0 found in the NoC census and 1.11.4
found in the CoreBreakdown split. And the power-gating endpoints from
1.11.8 arrive already family-priced, active and gated alike, so the
interpolation no longer needs to know about process families at all.

Scope is placement-derived, not global: rank/channel and base-die designs
put their logic on a buffer or base die and stay in the logic family.

Two things the gate caught before this shipped, both worth stating because
they are the release's own subject matter. The per-level NoC objects the
reporting layer reads were not transformed with the aggregate, so the
breakdown printed logic-process leakage under a periphery-priced total --
the exact aggregate-versus-parts drift this change exists to end, appearing
inside it. And the AREA factor was being applied to the fabric, which is a
category error: the pitch penalty is a TRANSISTOR claim, while the 1.10.5
census showed most tree nodes are single-child pass-throughs, i.e. wire,
whose pitch on a DRAM die does not follow the device. Applying it anyway
put a 16-PE add-on at 36 mm^2 against its own 40.78 mm^2 HBM3 die -- 88% of
the memory it is embedded in. Device factors (dynamic, leakage) now apply
to everything on the die; the pitch factor applies only where area is
transistor-dominated. The add-on lands at 28 mm^2, 69% of the die.

DATA IMPACT: device NoC, MC and cache power/area for subarray/bank/BG/chip
placements on DRAM technologies -- previously logic-priced. PE cores land
where they did. Timing untouched.

## 1.11.11 -- an element without an FP unit stops running FP for free

#113. pim.pe.floating_point=false removed the floating-point unit from the
POWER description and nowhere else, and the code said why: "the timing
model charges every instruction identically -- it never sees an opcode".
As of 1.11.10 it does. The decoder's class census travels with each basic
block, so an FPU-less element can now be told what it just executed.

Two things follow. The run REPORTS the contradiction with its measured
size -- "N FP-class instructions executed on an element declared WITHOUT
an FP unit" -- which no configuration could previously discover. And the
emulation becomes chargeable: pim.pe.fp_emulation_cycles adds that many
cycles per FP-class instruction in the timing model (through
sys.hierarchy.fpEmulCycles / peHasFpu into zsim). The default is 0 --
nothing charged, every existing run bit-identical -- because the honest
per-op cost of soft-float depends on the operation and the width (glibc
soft-fp is tens of integer operations), and inventing one constant for
all of them is the failure mode this train exists to remove. The knob is
offered, documented, and left to the user; what is NOT left to the user
is knowing that the situation occurred.

DATA IMPACT: none by default. With the knob set, timing and everything
derived from it move on FPU-less elements that execute FP code.

## 1.11.10 -- the instruction mix is counted, not assumed

#112. Every instruction the decoder handles already carries a class
(x86_decoder.h OpClass: ALU, MOV, LEA, IMUL, IDIV, FADD, FMUL, FDIV, FMA,
VECALU, VECMOV, BRANCH), and every one of those classifications was thrown
away the moment the uops were emitted. The class census is now recorded per
basic block at decode, accumulated per core at retirement (all five core
types, ROI-rebased alongside instrs), exported through zsim's stats tree and
parsed like every other activity counter -- and McPAT is fed the COUNTED
int/mul/fp split instead of the documented 87.5/12.5 stand-in that had been
holding its place since 1.9.28. Load and store uop counts ride along on the
same path.

The stand-in remains the fallback: a core model that never decodes (or a
class that measures zero) keeps the fractions, so nothing regresses where
there is nothing to count. And if the classified count ever exceeds the
retired non-branch count -- the two counters disagreeing on base, the defect
class 1.11.9 root-caused -- the fractions are kept and the disagreement is
printed rather than papered over.

This is what 1.11.9 unblocked: the mix-consistency gate had been rejecting
the measured set because instrs was latched from one core while the activity
counters were all-core sums. With both on the same base, the measurement is
usable for the first time.

Measured on a 16-PE in-order HBM3 cell: 152,799 integer, 113 multiply and
263,787 FP-class instructions of 487,424 retired -- about 63% FP where the
stand-in assumed 87.5% INTEGER. The assumption was not merely imprecise for
this kernel; it was inverted.

DATA IMPACT: core dynamic power wherever the real mix differs from
87.5/12.5 int/fp -- which is everywhere with FP content. Timing untouched:
the deterministic 1-PE cell is bit-equal, and the 16-PE cell's 4% gate
difference was shown to be the cell's own variance, not the release's --
the UNCHANGED 1.11.9 binary varies 6.11% run-to-run on that cell (the two
binaries' first runs agree to 0.03%). Bit-equality arms belong on
deterministic cells; the 1.9.41 finding, re-confirmed.

## 1.11.9 -- co-simulation reports what it measured

#86 and its audit sheet (six blockers). (1) System scope HARD-ABORTED on a
decoupled co-simulation -- host DDR5 plus device HBM3, the standard PIM
configuration -- because a 1.9.42 guard insisted the system had exactly one
memory. Each node's memory is now charged with ITS OWN technology and ITS
OWN counters, which is what the timing side has done since 1.1.0; a shared
memory (device IS the host's memory) names one technology and is priced
once, so those runs are unchanged. (2) The parser accumulated across EVERY
stats dump in zsim.out: zsim appends a full monotonic dump per write, so a
run with a periodic dump plus the final one reported double its accesses.
Only the final dump is read now, and multi-dump files announce themselves.
(3) cCycles was left absolute while cycles was ROI-windowed, and
host_wall_cycles summed the two -- cCycles is now rebased on the same
window. (4) The memory die's AREA never appeared in system scope, so a
co-simulated system's total silicon omitted its largest piece; it is
computed with the same JEDEC-calibrated CACTI model as device scope and
summed in. (5) The PE-MI locality split has been emitted since 1.5.3 and
read by nobody -- the one measurement that says whether a placement kept
its accesses local; it is now parsed and reported. (6) HOST_MC placement
produces no device memory group BY CONSTRUCTION (the elements sit at the
host controller); the report says so instead of printing a bare zero.

And the root cause of a mystery the code itself documented as "not yet
understood" (1.9.28): the parser latched instrs from the FIRST core while
uops, branches and syntheticInstrs were all-core sums. Measured on a 16-PE
HBM3 cell, that is 51,660 against a true 512,788 -- McPAT, which divides by
the core count, modelled every PE as doing a tenth of its work, and the
mix-consistency gate then rejected the measured instruction mix and fell
back to documented fractions. instrs is now summed like every other
activity counter. Cycles remain first-core: a duration, not work.

DATA IMPACT: device-scope core dynamic power rises where instruction-driven
terms dominate (the counter was ~10x low on a 16-PE cell); co-sim runs gain
memory area, per-node memory energy, and locality reporting; any run whose
zsim.out held more than one dump loses the multiplied counts. Timing
untouched.

## 1.11.8 -- power gating: one flag per component, physics per technology

#84, to the spec converged interactively (v7): NO global switch, NO
granularity knob -- the entire config surface is a per-component pg:
true/false at initialization (pim.pe.pg, noc.pg, pim.mc.pg; default
false, so a config with no pg: keys is bit-identical to 1.11.7, enforced
at the XML level). Residency is measured per component where its events
happen (active-phase marking: cores at retirement, shared caches at
access, the fabric at injection, MCs at request -- one compare+branch
per event, exported through zsim's stats tree), and each gated
component's effective leakage is the tool-certified interpolation
leak_eff = active*(1-r) + gated*r: the gated endpoint is McPAT/CACTI's
own sleep-transistor power_gated_leakage (dead plumbing repaired -- the
enable never reached CACTI before), DRAM descends its JEDEC ladder
(IDD2P precharge power-down during MC no-traffic residency, refresh
always on, tXP hysteresis stated as a 0.99 derate), and NVM periphery
gates retention-free to a 2% sleep-transistor floor (non-volatile cells
hold state unpowered -- the one place PG is free). Penalty accounting,
verified empirically at the gate rather than assumed: the sleep-
transistor LEAKAGE endpoints respond to the enable (gated != active !=
zero), but CACTI-P's sleep-transistor AREA overhead does NOT engage in
the vendored path -- pg-on and pg-off areas are byte-identical -- so
area overhead joins wake latency and entry/exit energy on the stated
unmodelled list (all bounded, all printed; revisit post-v2). The report prints per-component
active/gated/residency/effective plus the all-idle shared-domain
comparison line.

DATA IMPACT: none without pg: keys (bit-identical); with them, leakage
and DRAM background only. Timing untouched in all cases.

## 1.11.7 -- crossings are counted, and the link is priced from them

#85, co-sim half -- the audit's largest sheet (10 blockers). The plugin now
COUNTS every host<->device crossing at the moment it happens, bytes in hand:
WORK_BEGIN/END payloads, launch cmd/ack packets, and the coherence-flush
footprint, exported through zsim's stats tree (xingH2DBytes/xingD2HBytes/
xingCount/xingFlushBytes in zsim.out) and parsed latch-last (robust to
multi-dump files). McPAT's PCIe component -- kept per the model-borders rule
and made real: the fork's iocontrollers gains transferred_bytes +
link_pj_per_bit inputs, and runtime link dynamic is computed FROM the
transfer (bytes x 8 x pJ/bit, exact through the Processor's units x
clockRate aggregation; zero traffic = zero dynamic natively). Per-link-type
pJ/bit table (pcie_gen3/4/5, cxl = gen5 PHY + coherence delta, nvlink;
unknown types fatal unless power.pcie.pj_per_bit_override is given,
printed). Both ends of the link now carry a controller (the device end was
never priced); the readout reads the AGGREGATED pcies component (raw-object
read under-reported dynamic by units x clockRate); the link clock comes
from the configured link type, not a hardwired 350 MHz. All of it wired
into runPerNodePowerAnalysis -- the path a real co-simulation executes,
where interface energy was previously unreachable. And the synthetic
timing BBLs (WORK/launch/flush) no longer manufacture phantom instruction
fetch (bytes = 4 x cycles): co-sim timing and cache/DRAM traffic move with
this release BY DESIGN -- that traffic never existed.

DATA IMPACT: all co-sim cells (crossing energy now real; phantom fetch
removed from timing and counters). Device-scope cells untouched.

## 1.11.6 -- audit hotfix 2: CACTI's temperature rows are Kelvin-minus-300

Round 3 of the audit caught the 1.11.4 hotfix's own error: CACTI's I_off
table rows are indexed as (T_kelvin - 300) -- parameter.cc:175 compares
thermal_temp against temperature-300 -- so the model's 350 K is row 50,
and the "80C" row 1.11.4 read is actually 380 K. Corrected: 22 nm leakage
factor 6.8e-6 -> 1.0e-5, 32 nm 2.2e-6 -> 3.1e-6 (higher, still
retention-grade). The audit's column-basis objection (comm-dram is CACTI's
cell-access device, not the periphery) is answered in-code rather than
patched around: comm-dram is the only DRAM-process device column populated
in both tables (lp-dram is all-zero at 22 nm), its ~2.4x delay ratio
reproduces the UPMEM DPU band, and the 32 nm lp-dram alternative agrees on
delay while giving a 4.3x area ratio -- so 2.44/2.46 is the conservative
end of CACTI's own DRAM-process band. Stated as a proxy, because no tool
in the chain carries a true DRAM-periphery logic device.

DATA IMPACT: DRAM-periphery-family PE leakage only (factor ~1.5x up from
1.11.4, on a micro-watt base). Timing untouched.

## 1.11.5 -- the DQ pins are charged once, and only when they are crossed

The audit's interface-energy findings, memory side. (1) The 'interface'
term was vdd*(IDD4R-IDD3N)*tBurst -- bit-identical to the DQ burst current
already inside the array read term (JEDEC IDD4R is measured with outputs
driving), so every access was double-charged. interfaceNJ is deleted; the
interface term is now TERMINATION (ODT) -- the genuinely additional
off-chip energy, computed per I/O standard since 1.9.10 and never charged
until now (dead code resurrected). (2) Termination is placement-aware: an
access from an on-die PE (subarray/bank/BG/chip) never crosses the DQ
pins and carries none; RANK/CHANNEL/HOST_MC accesses do. HBM terminates
nothing at any placement (interposer microbumps -- the model's own zero,
by physics). (3) Writes consult IDD4W: the write burst term is
vdd*(IDD4W-IDD3N)*tBurst plus the activate share, replacing read*1.2.
(4) The total-dynamic line prints 'term=', not the false 'pre='.

DATA IMPACT: memory dynamic energy everywhere -- on-die placements drop
(double-charge removed, nothing added), off-die DDR-class placements
exchange the DQ double-charge for real termination, writes move by
IDD4W/IDD4R. Timing untouched.

## 1.11.4 -- audit hotfix: the leakage factor was read at the wrong temperature

The user's Opus audit fleet (five rounds over the open 1.11.x issues) turned
its first round on the 1.11.2/1.11.3 factor harness itself and found five
defects, all fixed here. (1) The leakage factors were derived from the 0C row
of the CACTI tables while McPAT runs at 350 K (~77C); the 80C row raises the
DRAM-periphery leakage ratio ~7-8x (22 nm: 9.0e-7 -> 6.8e-6; 32 nm: 2.7e-7 ->
2.2e-6). Still retention-grade -- but no longer understated. (2) The 22 nm
and 32 nm factors were derived with inconsistent formulas; both now use one
derivation (I_off*Vdd at 80C, cd/hp), stated in the code. (3) The factor was
applied on top of McPAT's longer-channel leakage discount; comm-dram is
already a long-channel device, so leakage is now rebased on the plain value
(no double discount; peak path identical). (4) On-die L2/L3 sit in the same
DRAM-die silicon as the PE but were still logic-priced; they now carry the
same family factors, power and area. (5) The CoreBreakdown block weights were
captured unscaled and printed against a scaled core total; they now carry the
core-power ratio. Plus: leakage fields NaN-guarded in extraction (the peak
path already was), the factor literals live in one place, and a basis
mismatch (device_type != hp with family factors) warns.

DATA IMPACT: DRAM-periphery-family PE leakage (up ~7-8x from a tiny base)
and on-die cache power/area for placements with device caches. Timing
untouched.

## 1.11.3 -- the periphery device is per-generation, and the istore is a RAM

Two refinements to the 1.11.2 process surface. (1) The DRAM-periphery factor
set is now read from the CACTI table the technology's generation class maps
to, not fixed at 22 nm: DDR3-class periphery uses the 32 nm hp/comm-dram
columns (area x2.46, dynamic x0.66, leakage x2.7e-7, delay ~1.5x), everything
newer the 22 nm columns (x2.44 / x0.82 / x9e-7, ~2.4x) -- different DRAM
generations now carry measurably different periphery devices. Gate-0 also
settled the mechanism question: pricing a whole PE natively in CACTI's
comm-dram device (device_type=4) is rejected by CACTI itself (UCA asserts
readOp.dynamic > 0; the retention device with Vth 1.0 V > Vdd 0.9 V only
works inside the DRAM-array machinery where wordline boost exists). The
factor harness is therefore the mechanism, documented as such. (2) #111: a
PE's instruction store is a resident RAM, not a cache. buffer_sizes[0]=0 now
marks it in the fork's InstFetchU: the array is built pure-RAM (no tag) and
the miss/fill/prefetch buffers -- structures a scratchpad does not have --
are not built at all. Previously the store carried a tag array and three
one-entry MSHR-class buffers whose combined overhead rivals a small imem
itself.

DATA IMPACT: device PE power and area wherever the ALU imem is priced (all
alu_core PEs, both families), and DDR3-placement PEs specifically via the
32 nm factor set. Timing untouched.

## 1.11.2 -- the process surface: every domain on its own silicon

Three related fictions removed. (1) The technology-node knob was clamped only
at the bottom: any value >= 22 passed through, so CACTI silently interpolated
unvalidated blends for intermediate nodes, and its 16nm.dat -- a 25-byte stub
reading "Invalid technology nodes" -- was reachable for 16-21 nm requests.
The knob is now a positive-list: 22, 32, 45, 65, 90 nm (the nodes every
linked tool evaluates from real tables), anything else a fatal config error.
(2) The logic node reached the DRAM die: setting a 45 nm host re-priced HBM3
arrays as 45 nm DRAM silicon. The DRAM array now derives its process from the
technology generation (DDR3=3x/2x on the 32 nm table, everything newer on
22 nm; printed), and power.tech_node_nm names logic domains only. (3) Every
PE was priced as logic-process silicon regardless of where it sits. A
placement x technology matrix now selects the process family: subarray/bank/
bank-group/chip PEs on DRAM technologies are DRAM-periphery devices, scaled
by factors read off CACTI's own 22 nm hp vs comm-dram device columns (area
x2.44 from l_phy, dynamic x0.82 from C*V^2, leakage ~0 from I_off -- the
retention-grade device is the physics, and Ramulator2 carries DRAM-die
background power). Rank/channel/LOGIC_DIE/host PEs and all PEs beside
SRAM/NVM arrays remain logic. Cross-check: the x2.4 CV/I delay ratio puts a
1 GHz logic PE at ~420 MHz, inside UPMEM's published 350-466 MHz band; a
warning fires above 700 MHz. Interim factors; 1.11.3 replaces them with a
real McPAT DRAM_PERIPHERY device family. Subarray placements gain
power.subarray_pitch_factor (default unity).

DATA IMPACT: device PE power and area for BANK/BG/CHIP/SUBARRAY placements
on DRAM technologies (most of the corpus); timing untouched. DRAM die area
unchanged at default configs (same 22 nm table as before for post-DDR3
technologies; DDR3 moves to the 32 nm table).

## 1.11.1 -- one array-area model: CACTI calibrated by JEDEC (user design)

Raw CACTI comm-DRAM areas failed physics (four technologies identical, HBM3
past a reticle); the JEDEC density figure is vendor-anchored but structurally
blind -- it cannot respond when a user reconfigures banks or IO. Each now
contributes what it is good at: k = JEDEC(preset org) / CACTI(preset org),
computed at the technology's own Ramulator2 organisation, and the reported
die area is CACTI(effective org) x k. At the stock organisation this is
exactly the vendor-anchored figure; under reconfiguration it moves by CACTI's
structural derivative. k is printed with the raw value, so a calibrated
number can never pass as a raw tool output. Fallback to plain JEDEC density
when CACTI fails.

Measured k across the seven technologies spans 0.05 to 1.2 -- a factor of 24,
the one-line demonstration of why raw CACTI could not be primary.

DATA IMPACT: reported DRAM die area only; power and timing bit-equal
(verified 10164674 cycles unchanged).

## 1.11.0 -- power and area price the tree that was built, at every level

1.10.5 fixed the top-level network inputs; the hierarchical (levels) machinery
was separate plumbing and never got the cure. It sized every tier from the raw
organisation count -- about 528 endpoints on a sixteen-element HBM3
configuration -- and so priced a 23x23 bus at the bank level and a 12x12 NoC at
the bank-group level: hundreds of routers, 53 of the device's 63 mm^2 and 1.3 W
of leakage, for a tree that actually built ONE branch router.

The tree now reports a per-level census of itself (branch routers and attached
endpoints per tier, derived from the builder's own bookkeeping), and the levels
machinery consumes it. A tier with no arbitration and no endpoints is wire and
is skipped, not priced. Non-DRAM configurations, which build no tree, keep the
old estimator. Measured on 16-element HBM3: NoC area 53.4 -> 5.3 mm^2, device
total 62.9 -> 14.8 mm^2, device leakage 1.8 -> 0.44 W. Timing bit-equal.

Also: the system-scope report now prints per-node area (host socket vs device
add-on) instead of one aggregate; CACTI's DRAM path no longer fails silently on
wide interfaces (block sized from the interface, the relation CACTI enforces);
and CACTI's comm-DRAM die areas are diagnostic only -- cross-checked against
JEDEC density figures they came back nonphysical (four techs identical at
20.9 mm^2, HBM3 at 756 mm^2, beyond a reticle), so the vendor-anchored density
path remains authoritative.

DATA IMPACT: device-scope power AND area for every DRAM configuration with the
detailed network. The phantom-router leakage was ~35% of device power on the
measured configuration.

## 1.10.6 -- the shared channel data bus pays to reverse direction

A DRAM channel's DQ bus is one road: reads and writes take turns, and turning
costs tWTR. A bandwidth-limited link knows the road's width but not the cost
of turning, so mixed read/write streams rode free. The expected penalty --
2 x P(read) x P(write) x tWTR, zero for any single-direction stream, exactly
as the bus behaves -- is now charged per access and lengthens the service
time the channel queue sees. tWTR is each technology's own, from the
Ramulator2 preset the run selects (DDR3/DDR4 7.5 ns, DDR5 10, LPDDR5 12.5,
GDDR6 6.27, HBM2 8.33, HBM3 8.11); the direction mix is measured from the
run's own traffic. Read-to-write is charged at the write-to-read figure, a
stated approximation. memory.dq_turnaround: false disables it, for designs
whose PIM interconnect is not a shared bus.

Two of this fix's own defects were caught by its gate before shipping: the
charge first sat inside the queueing term, where utilisation ~0.02 multiplied
it to zero; and the OpenMP path carried an un-rerouted duplicate of the
pricing formula, so the charge reached only the MPI path. The duplicate is
deleted -- both runtimes now price from one function.

DATA IMPACT: device-scope timing for every DRAM configuration with a mixed
read/write stream. Measured: stream_triad (2.4:1 mix) on HBM3, 1 element,
+4.1% cycles, deterministic. Single-direction streams and dq_turnaround:false
are bit-exact with 1.10.5.

## 1.10.5 -- power describes the fabric the timing model routes on

McPAT was handed one router per element on a ceil(sqrt(elements)) square grid,
and controllers by element grouping. The device contains neither: it has the
placement tree -- sparse, element x depth routers, most of them single-child
pass-throughs that are wire rather than arbitration -- and one
controller-worthy endpoint per region, aggregated regions included, because
memory that only responds still needs sequencing and refresh.

Power now takes the router count from the built tree and charges only branch
routers; pass-throughs are carried by their links. Controllers are counted per
tree endpoint. Sixteen bank-placed elements on HBM3 build one branch router and
fifty pass-throughs -- the square mesh billed sixteen routers for that
configuration, and billed the same fabric for every placement level, which is
the defect: placement changed the machine and power could not see it.

DATA IMPACT: device-scope power for every DRAM configuration with the detailed
network. Direction and size vary by placement (deep trees shed invented
routers; measured: 16-PE BANK/HBM3 3.9 W -> 3.8 W, CHIP unchanged at 3.0 W).
Timing is untouched -- verified bit-equal on the deterministic single-element
configuration.

## 1.10.4 -- the repository states the version it actually is

The README badge still read 1.10.0 while the binary reported 1.10.3, and the
changelog stopped at 1.10.0 -- so three shipped releases were absent from the
documented history and the public landing page advertised a version that had
been superseded twice. Same fault as the hardcoded --version string 1.10.2
fixed: a version written by hand, tracking nothing.

The badge and the changelog are now part of the release, and publish-public.sh
refuses to publish when the badge and the project version disagree, so this
cannot drift again unnoticed. The ledger is renamed changelog.md, since it long
ago stopped being about the 1.8-1.9 train.

## 1.10.3 -- the in-memory fabric is described as the memory is built

The default DRAM fabric was four virtual channels with four-deep buffers, which
describes a packet-switched router a DRAM die does not contain. It is now the
smallest description the routing needs: one UP and one DOWN channel, buffers of
two. Two rather than one, because the tree routing stays deadlock-free only by
keeping the two directions in separate VC classes; a request for one is warned
and lifted.

A link's width now follows the tier it crosses rather than its distance from
the leaf. Choosing by distance made the width depend on how deep the tree
happened to be, so the channel link -- the narrowest in the device -- was priced
with the width of a fat inner datapath whenever the placement was coarse, in the
direction that makes memory look faster than it is.

And noc.levels[<tier>].link_width_bits now reaches the detailed DRAM tree. It
was parsed and then ignored on the default path, so a user modelling a widened
channel silently received the stock part's numbers. A run under an override
says so in the output.

DATA IMPACT: invalidates device-scope network timing for every DRAM technology.
The direction is more cycles (narrower upper links, less buffering); the
magnitude is not established -- both execution models vary more run-to-run
(16% and 20% measured) than the effect -- and belongs to the corpus
re-simulation.

## 1.10.2 -- the repository carries sources, and says which version it is

Two compiled CACTI executables (2.5 MB each) and two upstream PDFs (3.6 MB) were
committed into a source repository; the build referenced none of them. And
--version had answered 1.8.0 since that release, because the string was written
by hand with nothing tying it to the project version. It now comes from
PROJECT_VERSION through a compile definition.

DATA IMPACT: none.

## 1.10.1 -- two configurations the model was letting pass in silence

Placing elements below the rank asks that rank for more concurrent row
activations than the JEDEC four-activate (tFAW) window permits. PIMID does not
refuse -- widening the window is a legitimate design -- but the timing model
does not enforce tFAW, so such a run is optimistic by whatever the window would
have serialised, and now says so.

Requesting a mesh, ring or crossbar on a DRAM device was already overridden to
the hierarchical tree, correctly, and silently. The override is now reported,
naming both the requested fabric and the one used.

DATA IMPACT: none. Both are print-only; single-element runs are bit-exact
against 1.10.0.

## 1.10.0 -- the tree now knows where a channel begins

The placement tree collapses regions with no processing element into a single
aggregated endpoint. That is the right idea: memory nobody computes in does not
need its own network interface, because an interface costs what its link costs
and not what sits behind it.

But nothing stopped the collapse from crossing a channel boundary, and on sparse
placements it did.

### What was wrong

Measured on a single-element stacked-memory configuration: one endpoint stood for
four hundred and eighty organisations. That is fifteen entire channels behind one
interface.

Channels are independent. Fifteen of them have fifteen data buses working in
parallel, and collapsing them into one destination throws that parallelism away.
Worse, the path out to each of them is below the network's endpoint and outside
the memory model, which only covers within a channel -- so nothing priced it at
all.

It stayed invisible because of a naming accident. The hierarchy's level names are
shaped after conventional memory: subarray, bank, bank group, chip, rank,
channel, system. A stacked part has no separate chip dimension, so its channel
count is stored in the chip slot. The aggregation was therefore happening at a
level called "chip", which reads as safely below "channel" while in fact BEING
the channel. The same folding produced a sixteen-fold coverage error earlier in
this release, wearing the same disguise.

### What changed

The channel tier is now always real: every channel gets its own router whether or
not a processing element lives in it. Aggregation then happens strictly within a
channel, where the memory model's coverage holds, and an empty channel gets its
own endpoint -- which is what it physically is, rather than a fifteenth of one
thing.

Which tier carries the channel is detected, not assumed: a technology that folds
its channels into a lower slot is recognised by the same test that caught the
coverage error, so no technology is special-cased by name and a future part that
folds the same way is handled without further change.

The cost is a handful of routers on sparse placements.

### Also in this release

Aggregated endpoints now record what they stand for -- how many organisations,
and at which tier -- and the tree is checked against the organisation count the
rest of the system uses. A mismatch stops the run rather than warning, because
every access cost and every energy figure is computed against this structure.

That check is deliberately made against the count the rest of the system uses,
and not against a second computation of the tree's own. An earlier version
compared the tree only against itself, agreed, and concealed the coverage error
described above.

### Data impact

Timing changes for sparse placements on technologies that fold their channels --
those configurations previously routed to an endpoint that stood for many
channels at once. Fully-populated placements are unaffected, because every
channel already held a processing element and there was nothing to materialise.
Conventional memory is unaffected at every placement, since it does not fold.

## 1.9.44 -- documentation for the whole train

One pass over the documentation for everything the preceding fifteen releases
changed, rather than an edit per release.

### The power and area reference was twelve lines

It named the tools and stopped. It is now an account of what is priced, by what,
and -- the part that was missing entirely -- **what is assumed rather than
measured**. The instruction mix is a fixed ratio because nothing counts
floating-point or multiply instructions; per-access interface energy is an
inherited constant; power gating has no temporal weighting of its own. Those
belong beside the results, not in a reader's inference.

It also records the reference class and what that choice does NOT claim. Pricing
an element against embedded parts rather than server processors is not an
assertion that in-memory logic is cheap. The opposite penalty -- logic in a memory
process being slower and larger -- is a real and still-missing term, and the
document says so rather than letting the change read as a discount.

And the memory rule: there is one memory, and it is charged once.

### What the element is, and is not

The core-model reference described the element as a minimal arithmetic unit
shaped by scaling factors. It is now described as what it became: a datapath with
a register file, arithmetic units, a result bus and a resident instruction store,
sized by its own parameters.

More useful to a reader: every core model consumes the same host instruction
stream, and the element does not decode. It models no instruction set and cannot
tell a floating-point operation from an integer one. What it does model is the
cost of an operation and the cost of reaching data. The memory side of an element
is modelled in detail and the compute side crudely -- which is defensible, since
processing in memory exists for memory-bound work, but it means conclusions about
compute-bound kernels do not follow from this simulator. That belongs in the
documentation and in any write-up.

### Reproducibility

Given a deterministic instruction stream the simulator is exact. A parallel
workload does not repeat, because the host schedules its threads and the emulator
reflects that faithfully -- the variation is the workload's. Its size is not a
constant and must be measured per study rather than quoted as a property of the
tool. Two consequences are stated: differences smaller than the variation are not
findings, and a regression can be gated bit-exact against a single-threaded run.

### A documented alias that no longer exists

The configuration reference listed two spellings of the element type as valid
that had been retired and are now rejected. A reader following the documentation
would have hit an error. Corrected, with the canonical name leading and the
retired ones named as retired.

### Data impact

None. Documentation only.

## 1.9.43 -- a summary line that had not moved with the rest

The instruction counter includes injected timing charges: when a core runs
without decoded micro-operations it advances itself by adding per-block charges
that are counted as instructions but are not code that ran. Every consumer moved
onto the corrected count in an earlier release -- the power model at both of its
call sites, and the per-node activity report. One printed summary line did not,
and still reported the raw total.

For a host that offloads its work and then waits, that raw total is almost
entirely injected charges, so the summary could claim hundreds of thousands of
instructions where a handful executed.

The line now reports executed instructions and names what it excluded. The
whole-run accessor that returns that figure did not exist -- only the per-group
one did -- which is how a single consumer came to compute the answer differently
from every other. It exists now, so the two cannot drift apart again.

### Data impact

None. Nothing downstream reads that line; it is printed, not consumed. Verified:
power and area unchanged.

Verified also that the change is reached: the first attempt at checking it ran a
configuration whose execution never touches that line, which proved nothing. The
second used one that does.

On the configurations we run today the printed figure is unchanged, because the
element model that reaches this summary never takes the synthetic path and its
injected count is zero. The correction matters for configurations where it is
not.

## 1.9.42 -- the memory was not being charged at all in a co-simulation

A co-simulated system reported no memory array energy. Not for the host, and not
for the processing device either -- although the identical configuration run in
device scope reported it. The system total was cores, caches, interconnect and
memory controllers, with the memory itself absent.

Nothing was missing from the models. The array energy computation lived inside
the device-scope path only, and the per-node path never called it.

### One memory, one charge

The ticket that tracked this said host memory energy was missing and should be
added. Building that literally would have been wrong. This simulator models one
host and one memory: the memory either IS the processing device, or it is a plain
main memory with no processing in it and the run is host-only. Either way there
is one array. In a co-simulation the host's accesses and the elements' accesses
land on the same silicon, so the charge is the sum of them, applied once. A host
term beside a device term would have priced that silicon twice -- the same fault
this release train has been removing elsewhere, introduced by the release meant
to fix a gap.

So the technologies the nodes name are checked rather than assumed. If two ever
disagree about what the memory is, the run stops and says so, because that is a
topology this build does not model and charging one of them while dropping the
other would misprice the system without any sign that it had. Several memories,
devices and hosts is a later extension.

The computation is written as its own small routine rather than moved out of the
device-scope path, so that path is untouched and its results are unchanged by
construction rather than by test.

Interface energy is deliberately not charged here. It is already charged
host-side in device scope, and it is an inherited constant rather than a
measurement, so adding it in a second place would compound an approximation
instead of a measurement.

### Data impact

Co-simulated cells gain a memory array energy term they did not have. Device
scope is bit-identical -- verified, and guaranteed by that path being unmodified.

Non-DRAM technologies in system scope say plainly that their array is not charged
there yet, rather than printing a number shaped like a DRAM one.

## 1.9.41 -- the simulator is deterministic; the workload is not

An investigation that ended somewhere other than where it started.

Repeated runs of one configuration, same binary, disagreed on simulated cycles by
a few percent, and two machines disagreed by considerably more. Total work was
stable throughout -- memory read counts agreed to a fraction of a percent, and
every leakage figure was identical to the digit. So the simulator was performing
the same operations and assigning them different times.

### What it turned out to be

Not the simulator. Running the same configuration against the SERIAL build of the
same kernel produces bit-identical results -- cycles and reads, every digit,
across repeated runs. Given a deterministic instruction stream the simulator is
exact.

The variation comes from the workload. The parallel build runs many threads, the
host kernel decides how they interleave, and the emulator reflects that
faithfully. A real parallel program on real hardware does not repeat its
interleaving either. This is therefore a property to state and bound, not a fault
to repair, and the earlier characterisation of it as a simulator defect was
wrong.

Three explanations were eliminated on the way, recorded so they are not
re-investigated: the emulator's host-derived default thread count is never
reached, because the count is set explicitly from the simulated element count;
the simulator was ALREADY running single-threaded in every measurement taken, so
its own threading was never a candidate; and the guest's thread count comes from
the program itself, not from the machine.

### What is fixed

Four settings that control the guest's parallel runtime were applied only to
workloads that DECLARED themselves parallel. The declaration defaults to serial,
and the reference kernels include parallel binaries that no configuration
declares -- so those workloads took the path the code's own comment had warned
about for as long as it existed.

Without those settings the runtime sizes its thread team from the host machine's
processor count, which under user-mode emulation is the count of the machine the
job happened to land on; it may resize that team mid-run; and its threads spin at
barriers rather than sleeping, so the simulator charges cycles for spinning whose
duration the host kernel decides.

They are now applied to every workload. They are inert for a program with no
parallel runtime, and they are necessary for one that has it, so correctness
should not depend on the user having declared the workload accurately. An
explicit environment entry in the configuration still overrides them.

Documented in the architecture reference -- what each setting prevents, that they
change results, and what they do not fix.

### Data impact

This one MOVES RESULTS. Enabling the settings on a configuration that previously
ran without them changed reported cycles by several percent and memory reads by a
fraction of a percent -- the reads because a runtime free to resize its team
distributes work differently. Numbers produced before and after are not
comparable, and every device-scope cell would need re-simulation to be brought
onto the new basis.

Run-to-run variation on parallel workloads fell by roughly a factor of five. It
does not fall to zero and cannot: the remaining variation is the workload's own,
as the serial comparison shows.

### A check that now exists

Any regression can be validated against the serial build and required to be
bit-exact. Every gate run before this one judged its result through noise that
did not need to be there.

## 1.9.40 -- the two halves must agree about what the element is

The previous release gave the processing element a datapath description. This
makes that description agree with the one the timing model already had, and
where the two cannot yet agree, says so out loud instead of leaving it to be
discovered.

### One width, one name

The element's datapath width already existed. The timing model has read it for
many releases, as the operand width, to charge a bit-serial datapath a step per
bit. The previous release gave the power model a SECOND field for the same
physical quantity and parsed it separately -- so asking for a wider element would
have produced a wide datapath in timing and a narrow one in power, silently.

That is precisely the fault this release train exists to remove, and it was
introduced by the release that removed several instances of it. The second name
is withdrawn; the power model now reads the field the timing model reads.
Configurations naming the withdrawn spelling are refused with a message pointing
at the surviving one, rather than having it ignored.

The width is also validated now. It was previously read only by the bit-serial
cycle charge, which clamps it upward, so a nonsensical value was merely inert; it
now sizes register files and result buses, where the same value would abort the
array model.

### Saying so when the halves cannot agree

Three cases remain where the two halves describe different machines and cannot
yet be reconciled. Each is now stated at the point of use rather than left
silent, because silence is how every defect in this train survived.

A datapath narrower than the power model's granularity. The power model works in
32-bit steps, so an 8- or 16-bit element -- a real in-memory design point the
timing model already supports -- is priced as 32-bit. Refusing it would remove a
capability the timing side has, so it warns and names what is overstated.

Lanes without matching throughput. Declaring lanes widens the arithmetic, the
register file and the result bus in the power model, but the timing model
expresses width through its throughput divider. Declaring one without the other
gives an element that pays for many lanes and runs like one. That is a legitimate
thing to model on purpose, so it warns rather than refuses.

An element declared without floating point. This removes the unit from the power
description only. The timing model never sees an opcode, so it will not charge
the software emulation a real part lacking that unit would need -- meaning the
element would run floating-point kernels at full speed with nothing to run them
on. Honest for an integer kernel, not for a floating-point one, and the model
cannot tell which is coming. It warns, and the underlying gap is tracked.

### Documentation

The three parameters added in the previous release were shipped undocumented.
They are documented now, together with a correction: the width parameter's entry
said it was ignored in the non-bit-serial case, which was true until the previous
release and is no longer.

The configuration reference also now states plainly what the compute unit is.
Every element model consumes the same host instruction stream; the compute unit
does not decode, so it models no instruction set and cannot distinguish a
floating-point operation from an integer one. What it does model is the cost of
an operation and the cost of reaching data. That belongs in the documentation
rather than in the reader's inference.

### Data impact

None. No default changes, no existing configuration changes meaning, and the
warnings do not alter any computed value. Verified: a default configuration is
bit-identical to the previous release and emits none of the new warnings.

## 1.9.39 -- the processing element is composed, not borrowed

The element's power and area came from a description of a server processor with
some fields turned down. This replaces that description with one built from what
the element actually is. Four faults, each independently validated.

### The reference class

McPAT prices a die against one of two measured populations. The default is
server processors: the undifferentiated-core term is a curve fitted to
Niagara, Niagara2, Merom, Penryn, Prescott and Opteron die photographs, the
functional units carry desktop areas and energies, and the wires are top-level
global. The other population is embedded parts, calibrated against ARM designs
and Sandia's parametrized-processor study.

The flag selecting between them was never emitted, so it took its default. Every
processing element in every sweep was priced as a fragment of a server die -- by
omission, not by choice. The undifferentiated term alone, evaluated for a short
element pipeline, exceeded everything the description actually named by orders of
magnitude, and it carried most of the element's power. All of it leakage: that
term has no dynamic component at all, which is why it never appeared in any
activity-driven breakdown.

Device scope now selects the embedded population; host scope stays on the server
one, because the host is a server part. This is not a discount applied to make a
number smaller. It is the other of the two populations the tool was calibrated
against, and it is the one a memory-die element belongs to.

Note what this does NOT claim. Nothing here says in-memory logic is cheap. The
literature is consistent that logic built in a memory process is slower and
larger than the same logic in a logic process, and that penalty is a separate
item, still open. This release removes a term that was wrong; it does not add the
one that is missing.

### An element that runs floating-point kernels had no floating-point unit

The compute unit declared one integer unit, no multiplier, and no
floating-point unit -- while the timing model ran three of the five kernels on
it, every one of them single-precision floating point. The two halves described
different machines: the timing side retired floating-point operations in single
cycles on hardware the power side said did not exist, so their energy was never
charged.

The arithmetic is now composed from the datapath: one of each unit per lane,
because a lane that cannot multiply or cannot do floating point stalls on the
kernels we run and the timing model charges no such stall. An integer-only
element remains available as a configuration choice, since two of the kernels are
integer throughout -- but it is now a choice, not a silent default.

### The instruction store was an uninitialised default

McPAT builds an instruction-fetch unit for every core unconditionally. The
element path emitted no parameters for it, so the store was constructed from the
parser's initialisation routine, which fills the entire configuration vector with
the literal value one. The element's instruction supply was a one-byte, one-line,
one-way cache. Nobody had ever looked at it.

It is now an explicitly sized resident instruction memory, direct-mapped, with no
misses -- the program is resident, so there is no refill path to charge. The size
is a configuration knob, because it is the axis that decides which kernels an
element can run at all: a command-driven in-bank engine and a programmable
near-bank one differ mostly here.

RESIDUAL, named rather than hidden: the store is still built by the cache
constructor, so it carries a tag array and single-entry miss, fill and prefetch
structures a scratchpad would not have. That overstates it. Removing them needs a
pure-RAM instruction store inside the fetch unit, which is the remaining half of
this item and is tracked as such.

### Datapath width

The width parameter, which the tool reads only to size register files, queue
entries and result buses, was emitted as sixty-four for every scope. A 32-bit
element therefore carried 64-bit registers and 64-bit result buses. Device scope
now states the element's own width.

### Configuration

Four knobs describe the element: lanes, element width, floating point, and
instruction-memory size. They existed in the power configuration but nothing ever
set them, so every element was described identically regardless of what was
simulated. They are now read from the configuration file, validated, and refused
when given a value the model would not honour. Every configuration written before
this release omits all four and receives the documented defaults, so no existing
configuration changes meaning.

### Data impact

Every device-scope and co-simulation cell moves. Element power and element area
both fall substantially at all three profiles -- compute unit, in-order and
out-of-order -- and the reduction is larger in area than in power. Direction and
cause are the same in each case: the undifferentiated server-die term is gone.

Host-side results are UNCHANGED, bit-identical in dynamic power, leakage and
area, which is the check that matters most here: the reference class follows the
scope and does not leak across it.

Validated separately: the reference class, the functional-unit composition, the
instruction store, the datapath width, that each new knob reaches the model and
changes the result, that an invalid value is refused rather than accepted, and
that the host half of a co-simulation does not move.

## 1.9.38 -- one out-of-order model, not two

The previous release added a separate out-of-order description for processing
elements, distinct from the host's. This withdraws it.

An out-of-order processing element is hypothetical. No shipping in-memory part
has one: the commercial near-bank processor is deliberately in-order with many
hardware threads, using thread-level parallelism rather than speculation to hide
latency, and the in-bank engine of the stacked-memory part is command-driven wide
arithmetic. There is therefore no silicon against which a distinct device variant
could be calibrated, and separating the two bought a name without a difference --
every speculative parameter still came from the same characterised-core
constants.

For a hypothetical machine, describing it as a well-characterised out-of-order
core is the most defensible reference available, and inventing separate device
parameters would introduce exactly the kind of unanchored constant this release
train exists to remove.

One inaccuracy is recorded rather than papered over: the surviving description
carries an instruction-set decode flag appropriate to the host, which a
processing element would not have. It is left in place because correcting it
alone would not make the rest of the description any more applicable, and noted
so the next reader does not mistake it for a considered choice.

### Data impact

None beyond the preceding release. Verified: an out-of-order device cell is
unchanged from that release, and in-order and compute-unit cells are unchanged
from before it. Note also that only two configurations in the corpus name an
out-of-order processing element at all, so the preceding release's change reaches
two exploration cells rather than any swept figure.

## 1.9.37 -- an out-of-order element was described as in-order

The device-scope power path chose between exactly two descriptions: compute unit,
or in-order. A processing element declared out-of-order fell to the second.

That single choice gates every speculative structure in the generated
description -- machine type, reorder buffer, instruction window, register
renaming, physical register count, load/store ordering, floating-point issue
width, and the reorder and rename activity statistics. An out-of-order element
was therefore described with no reorder buffer, no instruction window and no
renaming, while the timing model simulated all of it.

The per-node path used by co-simulation already selected an out-of-order
description correctly. Only the device-scope path did not, which is why the
defect appeared on device-scope cells and not on co-simulation ones -- and why it
survived: the two paths disagreed and nothing compared them.

Fixed by giving the element its own out-of-order description rather than
borrowing the host's, so the profile name no longer misstates what is being
priced.

### Data impact

Device-scope cells configured with out-of-order elements. The direction is
upward: those cells were previously charged for none of the speculative
machinery they were simulated as executing, so they were under-priced. In-order
and compute-unit cells are unchanged, verified against the preceding release.

## 1.9.36 -- the processing element is a compute unit, and its power is a curve fit

Naming, a parametric description, and a measurement that says why the description
is not yet enough.

### The element is a compute unit

"ALU core" was already inaccurate. Three of the five kernels carry
single-precision floating point and two carry integers, so the element has always
needed a floating-point unit. "Compute unit" is also the vocabulary of the field:
the in-bank engine of a shipped stacked-memory part is a programmable computing
unit. Naming the model after what silicon calls it makes it legible to that
reader.

The distinction that matters is that a compute unit is a DATAPATH, not a
processor: a register file, arithmetic units, a result bus and an instruction
store, with no speculation, no dynamic scheduling and no caches. The power tool
offers exactly two core models, out-of-order and in-order, and BOTH describe
processors. That is why no core model fits this element.

The former spelling remains a supported alias, not a deprecation: it names the
entire configuration corpus, and dropping it would invalidate every cell ever
run. The bare short form is retired, having been used by nothing.

Adding the new spelling exposed a defect in passing: core-type normalisation
exists TWICE, once for device nodes and once for the processing-element section,
and neither calls the other. Adding a spelling to one left the other rejecting
it. Both are updated and the duplication is marked for the configuration
tidy-up; until then they must move together.

### The description is now parametric

The generator branched only on whether the profile was out-of-order, so a compute
unit and an in-order core produced BYTE-IDENTICAL input and the element was
charged for a branch predictor, caches, address translation and a scheduler it
does not have. The description is now its own, and parametric: the register file
scales with lane count, floating-point issue follows whether the element has a
floating-point unit, and the load/store path is request issue without queues or
caches.

### And a measurement that says this is not the lever

Reporting the tool's intra-core split required transporting it out of the
subprocess the power computation runs in, whose output is deliberately discarded.
With that in place the split is measurable for the first time, and it says
something uncomfortable: the modelled blocks are UNDER ONE PERCENT of element
core power, at every profile. The remainder is the tool's "undifferentiated core"
term -- a regression on pipeline depth, fitted to commercial parts three
technology generations older, whose in-order branch DECREASES with depth and
reaches zero at a depth shallower than a modern core. A short datapath pipeline is
therefore evaluated off the low end of that fit, where it is largest.

Declaring an element simpler makes it cost more. That is the opposite of the
intent, and it is why sizing the structures correctly changed nothing measurable:
the parametric description above demonstrates its own insufficiency. Confirmed by
experiment, not argued -- a fourfold reduction in register file and buffers moved
no reported figure.

The consequence is a method result rather than a number: an element's power
cannot be corrected by describing the element better, because the dominant term
does not read the description. It has to be composed from the tool's primitives,
which carry no undifferentiated term. Recorded rather than hidden, and the
reported split now states its own denominators so the share cannot be misread as
a removable fraction.

### Data impact

None. The naming is an alias, the parametric description moved no reported
figure, and the split is diagnostic output that nothing consumes.

## 1.9.35 -- a control that could never be set, and a dimension counted twice

Two faults in the in-memory hierarchy description, neither of which changes any
result today and both of which would have, silently, the moment the tree grew.

### A control wired end to end with no way to reach it

The number of ranks per channel is declared in the configuration structure,
written into the generated simulator configuration, and read by the execution
plugin, the trace driver and the analytical hierarchy model. It has no key in any
configuration file. It could therefore never hold anything but its default of
one, so the rank tier of every tree ever built has been a router with exactly one
child -- a pass-through latency hop -- while the surrounding code reads as though
multiple ranks were supported.

The plumbing was complete except for its first link. That link is now in place.
The default is unchanged, so no existing configuration moves.

### The channel dimension booked twice

For stacked memory the chips-per-rank figure is set to the number of channels in
the stack, as its own comment says. The channel count is ALSO supplied separately,
as the fanout at the root of the tree and as the concurrency multiplier on link
widths. The same physical dimension therefore appears in two places.

Nothing multiplies them today. The rank tier is degenerate, and the organisation
size is computed without a channel factor, so every processing element resolves
to the first channel of the first rank and the duplication cancels. It stops
cancelling the instant either tier is given real fanout, and the result would be
a silent eight- or sixteen-fold inflation of the tree -- in the direction that
makes the fabric look larger and costlier than it is.

Rather than restructure the tree, which belongs with the interconnect fidelity
work, the combination is refused: asking for more than one rank per channel on a
stacked technology now fails, naming the inflation factor and what would have to
change first. A latent contradiction that cancels by accident is not a safe thing
to leave for a later reader to rediscover.

### A comment that described something the code does not build

The generated topology was documented as encoding parallel channel subtrees. It
does not. With the rank count fixed at one and no channel factor in the
organisation size, both the channel and the rank routers have a single child. The
channel count is used only as the root fanout in the abstract-endpoint test and as
the link-width multiplier. The comment is corrected, because a false description
is what would let the duplication above read as deliberate.

### Data impact

None. Verified: with a technology and configuration unchanged, hop counts and
reported power are identical either side of this release, measured against a
reference rebuilt from the immediately preceding release including its execution
plugin. The new control defaults to the value that was previously hard-wired, and
the refusal fires only on a combination that was never expressible before.

## 1.9.34 -- interconnect distances were measured between the wrong nodes

The custom-topology hop count walked a router adjacency using endpoint
identifiers. Those are two different numbering spaces, and nothing translated
between them, because the topology reader discarded the lines that carry the
mapping.

The topology file describes three things: how many routers exist, how many
endpoints exist, and two kinds of link -- internal links between routers, and
external links attaching an endpoint to the router it hangs off. The reader
consumed the counts and the internal links and ignored the external ones. No
endpoint-to-router mapping therefore existed anywhere in the simulator, and the
shortest-path search was handed endpoint identifiers to index a structure keyed
by router identifier.

The bounds check in front of that search could not have caught it, and this is
the part worth recording. In a sparse tree the endpoints are FEWER than the
routers, so every endpoint identifier satisfies a test written to reject values
too large for the router array. The guard passed, the search ran between two
unrelated routers, and it returned a plausible distance rather than falling back
to the safe default. A limit that fires only when the wrong identifier space
happens to be the larger one is not a check.

Fixed by keeping the external links as an endpoint-to-router map and translating
both arguments before the search. Identifiers with no mapping still fall back.

Measured on a device-scope cell: packet count and device read count are
IDENTICAL either side of the change, and only the distance credited to each
packet moves. That is the signature the fix should have -- the traffic is the
same traffic, previously walked along the wrong paths. Total hops rise by about
a third, and average hops per packet go from roughly three to roughly four,
which is the physically sensible figure for a tree of this depth; three was too
short for the hierarchy actually built. Interconnect power rises with it, so the
fabric had been under-charged.

### Data impact

Interconnect power and energy wherever the custom topology is used, which is
every technology that builds the placement-driven tree. Hop counts feed the power
model directly, and they were wrong in the optimistic direction. Timing is
unaffected and was verified so: hop counts are a statistics and power quantity,
not a scheduling input, and the packet and access counts are unchanged.

## 1.9.33 -- results that were discarded, never emitted, or guessed at

Eight defects, all of one family. In each case the simulator either had the answer
and threw it away, or did not have it and supplied something plausible instead of
saying so. None of them announced itself; every one produced a number that looked
like a measurement.

### Area was computed, transported, and then discarded

Every reported area was zero, in both scopes, for as long as the artifact tree
records. Not a units error -- that was a different defect, fixed earlier, and the
conversion here is correct. The power computation is run in a forked child for
crash isolation. The child computes the areas and returns them through the result
blob; the parent caches them. But the two accessors still tested the child's
processor object, which in the parent is permanently null, so they returned zero
and the cached values were never read. Every other cached field is read without
that guard, which is exactly why power survived and only area was lost. Both
accessors now test whether the computation completed.

The number this reveals is NOT yet trustworthy, and is documented as such: it is
far larger than the modelled part can plausibly be, for reasons already recorded
as separate defects -- an ALU processing element is priced as a complete in-order
core, and the interconnect is priced over the full organisational tree rather than
the sparse one actually simulated. Area is therefore useful right now as a
DIAGNOSTIC for those two, and must not be quoted as a result until they are fixed.
What the fix does establish is that the machinery is sound: area scales with the
square of the technology node, as it must.

### The device's memory interfaces were never emitted

In co-simulation the processing-element memory interfaces are constructed, wired,
and incremented on the live path -- and then dropped from the list that the sole
statistics-registration loop walks, because that list is cleared so the host's own
controllers can occupy it. They survived only in a side vector kept for wiring. No
device-side memory group therefore appeared in a co-simulation dump at all. The
power model priced the device at zero memory-controller activity while charging
the host's controller for traffic that was partly the device's.

They are now registered from that side vector, in their own aggregate: the
statistics backend requires an aggregate whose children are all the same type, and
the host's controllers already occupy the existing one. This is the largest
numerical correction in the release by a wide margin -- the device's controller
traffic had been priced at nothing.

Note that an earlier release fixed only the READER half of this same problem,
teaching the parser the name the emitter actually uses. Both halves were needed
and only one had landed.

### A node that did nothing was treated as a node we knew nothing about

The per-node power path fell back to a core-count-proportional guess whenever a
node reported no activity. But "this node performed no work" and "we have no
measurements for this node" are different statements, and only the second warrants
a guess. A co-simulation host executes no code during the offload window -- it
prepared its data beforehand and is waiting -- so its measured activity is
legitimately zero, and the fallback then invented instructions for a core that
provably executed none. The test now asks whether the node was OBSERVED, not
whether it was busy. The fallback remains for what it was meant for: a statistics
file with no per-node breakdown at all.

### Substitutions that were made silently

Three more of the same shape, each now stated rather than assumed.

A requested technology node below the floor of the linked models was raised to the
floor without comment, so a study sweeping finer nodes would have received
identical numbers at every point and read as "technology does not matter here".
The substitution is still made -- there is no model to fall back to -- but it now
names the value that was ignored. Note the floor applies to the consumers of those
models: logic, static memory, non-volatile arrays, caches, the interconnect and the
memory controller. Array energy for dynamic memory is keyed on the technology name
through its own per-standard tables and does not read the node at all, which is
correct: those generations are not named in logic-process terms.

A destination-set entry beyond the addressable limit was DROPPED in silence, which
would simulate a fabric quietly missing those endpoints. It now refuses.

A configuration key that reads as a choice of memory-controller model accepted any
value while exactly one implementation exists, the alternatives having been merged
some releases ago. It now says the value had no effect.

### A technology we do not support would have been priced as one we do

The classification of a memory technology into "dynamic memory, priced by the
per-command model" or "everything else, priced by the cell-level model" was written
by EXCLUSION at three sites: anything that was not one of four named non-dynamic
technologies became dynamic memory. Any unrecognised string -- a typo, or a
technology deliberately not supported -- therefore passed through and was priced
with dynamic-memory command tables. A fourth site used the opposite, inclusive
form, so one string could be classified differently at different points in a single
run.

An earlier defect was this same failure for a letter-case mismatch, and was closed
by normalising case without removing the exclusion logic that permitted it. The
normaliser -- the single point every technology string passes through -- now
validates against the supported set and refuses an unknown one, naming what is
accepted. The three exclusion tests are left alone deliberately: each is correct
for canonical input, and validating at the entry changes no existing
classification.

Relatedly, a wrapper header advertised support for three technologies that no
configuration string can reach. The claim is corrected; those remain deliberately
unexposed.

### What this does not establish

Two of these corrections make a previously invisible quantity visible, and
visibility is not accuracy. The area figure in particular is now reportable and
still wrong, for causes recorded elsewhere. The device memory term is newly priced
rather than newly verified.

### Data impact

Device-side memory-controller power and energy in every co-simulation cell, which
had been zero -- the largest change here, and it moves system totals materially
rather than marginally. Host and device shares shift with it. Any node that
legitimately idled was previously credited with invented activity and is now priced
as idle. Reported area changes from zero to a number in both scopes. Timing is
unaffected: the statistics are read after the simulated process exits, and the
statistics-registration change adds a group without altering any scheduling
decision. Configurations naming an unsupported technology now fail where they
previously produced dynamic-memory numbers; no supported technology changes
classification.

## 1.9.29 -- the power model was fed counts that were absent, misattributed, or on the wrong base

1.9.28 corrected the instruction count that reaches the power model and left everything
else it consumes untouched. That was not a complete fix; it was one of several, and
stopping there made another of them worse. This release finishes the work and withdraws
a claim 1.9.28 should not have made.

### Counters that were never read

The configuration writer names each node's caches after the node, so a host's first-level
data cache is emitted under a node-prefixed name and its per-core instances likewise. The
statistics reader matched scope headers against the bare names only, which a prefixed name
never satisfies. No cache scope was ever entered in system scope, every cache counter
parsed as zero, and all cache dynamic power in every co-simulation cell was priced at zero
activity. The device-scope writer emits bare names, so that path parsed correctly and the
defect stayed invisible there.

The same failure appeared a second time, on the memory side. The device processing-element
memory interface registers under one name and the reader tested for a different one that
nothing emits. In device scope those interfaces are the only memory group present, so read
and write counts parsed as zero and the memory-array energy report showed no dynamic
energy at all -- while the same dump carried the full read and write traffic in the groups
the reader had skipped. Published memory energy was not affected: the analysis path reads
the raw counters out of the log rather than trusting that line.

Both are the shape of the interconnect key mismatch fixed in 1.9.24: a name that changed
on one side of an interface and not the other, producing zeros that read as an idle
component rather than as an error. Three instances in one release train is a pattern, and
a name-contract check between the emitters and the reader is recorded for 1.16.

### Counts that were apportioned rather than measured

Everything the model consumes except the instruction count was still divided between nodes
in proportion to their CORE COUNTS: cache accesses at every level, memory-controller
accesses, and the activity counters 1.9.28 had just begun to read. That is not an
attribution. It is an assumption that every core in the system performed identical work,
which is precisely false for a host driving a device.

The reader now learns each node's name from the core-group headers it already parses,
strips that prefix before matching a cache scope, and accumulates every counter into the
owning node's own set as well as the all-nodes total the device-scope path uses. Each node
is priced from its own set; where a node reports no counters the previous behaviour is
kept, so nothing regresses.

Two consequences are worth stating separately. The device is no longer charged for the
host's memory traffic -- under the old split a many-element device beside a single-core
host received most of the host's controller accesses, while its own memory is priced by
the memory model, so it paid for its own memory through one model and most of the host's
through another. And the host node in the device-scope reporting path is no longer
fabricated: that block runs whenever the scope is system, which is exactly when measured
host counters exist, and it derived all of its inputs from fixed divisors of the DEVICE's
instruction count. A comment promised they would be overridden by real statistics when
available; no such override existed.

### A base mismatch, and a claim withdrawn

1.9.28 said it had replaced an invented instruction mix -- fixed shares integer,
floating-point and branch -- with measured counters. It did replace it. What it replaced
it with was worse, and the release note describing it as an improvement was wrong.

The activity counters and the instruction count were not on the same base. The instruction
count was reported through the region-of-interest window; micro-ops, basic blocks and
mispredicted branches were reported as raw whole-run totals. Every ratio formed between
them divided a region-of-interest numerator by a whole-run denominator, which is why some
dumps reported more basic blocks than instructions -- an impossibility that should have
been caught before the counters were used.

Two further problems sat on top. The out-of-order core ran a branch predictor but exported
only its misses, never a branch total, so the mix was built from the basic-block count on
the reasoning that a block terminates at a control transfer. That holds for a real
multi-instruction block; on this decode path a block averages close to a single
instruction, so the block count is nearly the instruction count and the resulting mix
described almost every instruction as a branch -- in device scope it saturated, leaving no
integer and no floating-point operations at all. Separately, the plugin charges coherence
flush, kernel launch and barrier latency by manufacturing basic blocks whose instruction
field carries a CYCLE COUNT, and those cycles land in the instruction count
indistinguishably from executed code.

Fixed by putting the activity counters on the same window as the instruction count in both
core models; by giving the out-of-order core the branch counter it lacked, incremented
where the core already tests for a branch so the predictor call and its ordering are
untouched; and by tracking the injected timing charges separately so the power path can
subtract them. The reported instruction count keeps its existing definition, because it
also feeds the reported instructions-per-cycle and the cycle-based gates, and changing its
meaning would move published numbers.

After the fix the branch rate and micro-ops per instruction are both physical, on
out-of-order and in-order processing elements alike. In co-simulation the host is shown to
execute essentially nothing during the offload window, nearly all of its former
instruction count having been injected timing charges. That is architecturally correct --
the host prepares its data before the region begins and then waits while the device
computes -- and it had previously been described to the power model as executing that
entire count.

Where the counters cannot be reconciled the measured set is refused rather than repaired.
It is used only when the branch count does not exceed the instruction count and the
micro-op count is not below it; otherwise the documented fractions are used and the output
names the offending values. No scale factor was introduced to force agreement, because a
factor chosen to make a ratio look right is the class of invented constant this train
exists to remove.

### What this does not establish

The inputs are now measured rather than absent, misattributed, or drawn from a different
window. That is a correction of inputs, not a validation of the model. McPAT remains
analytical and unvalidated against silicon here, and the absolute figures should not be
read as verified. Several specific errors are gone; the confidence interval around the
result is not thereby known.

### Data impact

All system-scope power and energy from every generation before 1.9.29. Cache dynamic power
was zero and is now non-zero. Host and device shares of the core, cache and controller
terms all change. Device-scope memory dynamic energy was reported as absent and is now
measured. Core power from 1.9.28 onward additionally carried the degenerate mix described
above and is superseded by this release.

Timing is unaffected on every path. The statistics are read from the output file after the
simulated process exits, so nothing here can feed back into the simulation, and the core
changes add counters without altering the predictor call or any scheduling decision. This
was verified by repeated measurement against the previous release rather than by argument
alone.

### Still open, recorded rather than fixed here

The controller for every non-DRAM technology is parameterised as though it drove an early
DDR generation, so the emerging-memory technologies share one interface description that
describes none of them (1.9.30). The host's own memory-array energy is not modelled
anywhere: its controller is priced and the memory behind it is not (1.9.31). An ALU
processing element is priced as a complete in-order core, including an instruction fetch
unit, branch predictor, caches and a floating-point unit it does not have (1.9.32). Area
reports zero in both scopes, root-caused to a stale null guard left behind when the power
computation became subprocess-isolated (1.11). And the reported instruction count for a
co-simulation host remains contaminated by injected timing charges; only the power path is
corrected.

## 1.9.28 -- core power was priced on activity that was absent, invented, and misattributed

The concern that opened this item was that host per-core dynamic power looked
implausibly low for an out-of-order core at the configured clock. It was.
Three separate defects fed the core power model, each independently sufficient
to produce the symptom.

FIRST, the instruction count never arrived. The system-scope (co-simulation)
power path computed a per-node instruction total and then never passed it to
McPAT -- it supplied cycles and busy-cycles only. The count therefore kept its
constructor value of zero, and every core activity statistic in the generated
input was zero: integer, floating-point, branch, committed, and reorder-buffer
reads alike. Core dynamic power was approximately zero BY CONSTRUCTION in every
co-simulation cell. The device-scope path always supplied it.

SECOND, the instruction mix was invented. The activity statistics were built
from fixed fractions of the instruction count -- a fixed share integer, a fixed
share floating-point, a fixed share branch, and a fixed mispredict rate --
applied identically to every workload. A stencil, a graph traversal and a
matrix-vector product were all described to the power model as the same
instruction stream. The simulator measures micro-ops, basic blocks and
mispredicted branches, and none of it was being read.

THIRD, and the largest, work was misattributed between nodes. The per-node path
took a SINGLE core's instruction count and divided it across nodes in
proportion to their CORE COUNTS. With a single-core host beside a many-PE
device, the host was credited with a small fraction of its own work while the
device was credited with a large multiple of work it never performed. The
parser already separated host from device cores for cycle counts; it simply did
not do so for instructions.

Fixed by supplying the count, by reading the measured activity the simulator
already reports (branch counts derived from basic blocks, since a block
terminates at a control transfer, and mispredictions measured rather than
assumed), and by giving each node its own measured instruction total. The
integer/floating split remains an assumption, since retired operations are not
classified, but it now applies to the non-branch remainder rather than to the
whole stream.

Verified on a co-simulation cell: each node is now priced on the instruction
count it actually executed, the host's rising and the device's falling to their
measured values, and reported dynamic power rises accordingly.

What this does NOT establish: that the resulting absolute figures are right.
The inputs are now measured rather than absent or invented, which removes three
specific errors; the underlying power model remains analytical and its output
has not been validated against silicon. Treat the improvement as a correction
of inputs, not as a validation of the model.

Data impact: all core power and energy figures change in system scope. Host
figures rise substantially, device figures fall, and the split between them
changes. Device-scope runs are unaffected by the attribution defect but do gain
the measured instruction mix. Not comparable with earlier generations.

## 1.9.27 -- cycle timestamps exchanged between ranks now carry their clock domain

Defect: ranks exchange rendezvous timestamps as raw CYCLE counts, and the
receiver differenced a sender's stamp against its own clock with no conversion.
A cycle is not a unit of time; it is a unit of a particular core's time. The
subtraction is therefore valid only when both cores' clocks tick at the same
rate -- a property of the configuration file, not of the code. Where the rates
differ the computed wait is wrong by exactly their ratio, and nothing detects
it: the arithmetic succeeds and produces a plausible number.

This was correct for the configurations we happen to run rather than correct by
construction, which is the same failure shape as the NoC statistics parser
earlier in this train: a value that looks right until the assumption behind it
stops holding.

When it can fire today: ranks normally migrate between the host and the device
together, so they share a clock domain and the arithmetic happens to be sound.
The exception is the migration window, where one rank can already be on a device
processing element while another is still on the host. Those two domains
genuinely differ.

Fix: the published timestamp now carries the clock rate it was taken at, and
both consumers -- the rendezvous advance and the receive-side arrival
computation -- convert the sender's stamp into the local domain before
differencing. The rate is written into the padding word that already existed in
the parameter block shared between the guest transport and the plugin, so the
structure's size and field offsets are unchanged and the two definitions stay in
agreement. A missing or equal rate converts to the identity, so the previous
behaviour is preserved exactly wherever the assumption held.

Validated as INERT, which is the appropriate test for a change that should only
engage in configurations we do not currently run: the device-scope determinism
gate reproduces its 1.9.26 values across all five pinned and all five unpinned
control runs, and the co-simulation cell reproduces its 1.9.26 cycle count
exactly, not merely closely. Any movement would have indicated the conversion
engaging where it should not.

Data impact: none for configurations whose host and device clocks coincide,
which is all present ones. Configurations with differing clock rates were
previously computing rendezvous waits incorrectly and will change.

## 1.9.26 -- revert the wake-up snap only where the core is the rank's alone

Restores the transport-wait correction that 1.9.23 removed, this time gated so
it cannot consume another rank's execution. It closes the host-side defect
1.9.21 attempted, without the device-scope regression that forced that release
to be reverted.

Background. Waking a parked rank, the simulator does not set its clock to when
its message or barrier released it: it snaps the clock to the global phase
clock, wherever the simulation as a whole has reached. That value depends on
how far every other simulator thread progressed while this one was parked, so
it imports the running machine into the simulated timeline. The correction is a
PAIR -- revert the snap, then charge the real wait computed from arrival times.
Both halves are required; 1.9.21 removed the first and inflated every
device-scope result.

The revert measures the snap as "clock now minus clock when I blocked". That is
this rank's snap only if the core was ITS ALONE for the whole window. On a
device PE it is, one rank per PE since 1.9.20. On the co-simulation host core,
shared by every rank, other ranks ran on the same clock meanwhile, so the
difference is the snap PLUS their execution -- and reverting it destroys that
work, which is the host-side collapse.

So the revert now requires exclusive ownership: the same core throughout, and
no bind by a DIFFERENT thread in between. The second condition is what the
previous attempt got wrong. It counted every bind to a core, but a rank parks
at a barrier and rebinds to its OWN core on waking, which bumped the count and
made every rank appear to share. Tracking the core's current owner and counting
only foreign binds distinguishes "another thread was here" from "I came back".

Validated on BOTH gates together, which is the pairing 1.9.21 failed and the
two attempts after it could not achieve:

- device scope: the determinism gate returns to its pre-1.9.21 values across
  all five pinned runs and all five unpinned control runs;
- co-simulation: the oversubscribed host configuration reports a plausible
  instructions-per-cycle rate on two independent node types, where before it
  reported an impossible one.

Measured evidence is kept with the maintainers.

Data impact: co-simulation host cycles and host energy change on cells with a
substantial host phase, since the wait correction now applies there correctly
rather than discounting co-resident ranks. Device-scope results are unchanged
from 1.9.23, by construction and by gate.

The durable fix remains to set the clock to the computed wake-up time directly
rather than correcting a snap after the fact. An absolute write cannot consume
another rank's work because it is not a subtraction, and it makes the
shared-versus-exclusive distinction irrelevant. That requires rebasing the
core's pending weave state and is scoped separately.

## 1.9.25 -- stop fabricating host NoC activity

The per-node power path gave every node's network a hardcoded duty cycle with
no traffic behind it. For a device node 1.9.24 replaced that with measured
Garnet activity. For a HOST node there is nothing to replace it with, and the
placeholder was left billing a fixed fraction of peak to a fabric that may not
exist at all: the co-simulation host is single-core by default, where the
on-die network is degenerate (core to caches to memory controller is direct),
yet it was priced as if partially busy.

PIMID does not model a host-side interconnect. The device statistics describe
the in-memory network and pricing a host socket from them would be worse than a
placeholder, so there is no measurement to substitute. The honest treatment is
zero activity: the two-level structure is still emitted, because McPAT's
homogeneous-NoC path crashes CACTI below two levels, so the entry now carries
its leakage and nothing else.

Where a host fabric WOULD carry traffic -- a multi-core host -- the run now says
plainly that the interconnect is not modelled and its activity is not measured,
rather than reporting an invented figure. A single-core host reports that its
fabric is degenerate and priced at zero.

This continues the theme of 1.9.24: a plausible-looking number with nothing
behind it is worse than an explicit gap, because it cannot be audited.

Data impact: host NoC power changes on every system-scope cell. The term was
small, so totals move little, but it is no longer fabricated.

## 1.9.24 -- NoC power was never measured: the stats parser matched the wrong keys

Defect: every NoC power figure the simulator has ever reported was priced from
zero network activity, in BOTH device scope and co-simulation.

Root cause, one line. The Garnet statistics writer emits DOTTED keys
("garnet.total_packets = N"), while the reader compared against bare names
("total_packets"). No comparison ever matched, so every field kept its zero
default. The file was found, opened and read to completion without error, and
yielded nothing -- a silent failure with no warning anywhere. Both power paths
call the same parser, so device-scope sweeps and co-simulation cells were
equally affected.

Consequences that follow from that, each of which had looked like its own
defect:

- Co-simulation fell back to a placeholder NoC activity level, since the
  measured stats it tried to use always parsed as empty. The placeholder was
  not a lazy default; it was covering for a parser that could not return
  anything else.
- Device scope built its NoC levels from measured traffic as designed, but the
  measurement handed to it was always zero, so the levels carried no activity.

Fixed by stripping the dotted prefix before matching, tolerating any prefix
rather than only "garnet.".

Three further corrections in the same area, all of the form "use the
measurement that was already on disk":

- NoC access counts now come from the recorded HOP count rather than the
  end-to-end packet count. McPAT's NoC access statistic counts router
  traversals, so a packet crossing several routers is several accesses;
  counting packets understated activity by the average hop count.
- Flit width now comes from the recorded value instead of a literal.
- The network clock now comes from the recorded value instead of the PE clock,
  which is a different quantity.

Co-simulation additionally now sources its NoC levels through the same builder
the device path uses, instead of emitting a fixed two-entry placeholder. When
usable statistics are genuinely absent it still falls back, but now says so
explicitly rather than reporting a placeholder figure as if measured.

Data impact: every NoC power figure changes, in device scope and
co-simulation alike, because the term was previously priced from zero activity.
Reported totals rise accordingly. This does NOT affect timing -- the parser
feeds the power model only -- so cycle counts are unchanged. Results are not
comparable with earlier generations for power or energy.

Version note: 1.9.22 was planned for this work but never released; 1.9.23
shipped first as a revert, so this lands as 1.9.24 and 1.9.22 is skipped.

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
