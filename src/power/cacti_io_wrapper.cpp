/* PIMID 1.11.40 -- CACTI-IO harness. See include/power/cacti_io_wrapper.h. */
#include "power/cacti_io_wrapper.h"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <unistd.h>

#include "extio.h"
#include "extio_technology.h"
#include "parameter.h"

namespace PIMID {

namespace {

/* SPECIFICATION PRIMITIVES ONLY.
 *
 * These are the values a standard fixes exactly, and they are the only kind of
 * single number that belongs in a table: a transfer rate, an encoding ratio, a
 * default lane count. Everything downstream -- bandwidth, energy, area -- is
 * COMPUTED. That is the difference between this table and the one it replaces,
 * which held 63.0 GB/s and 7.0 pJ/bit as if they were facts about a standard
 * rather than outcomes of a design.
 *
 * enc_num/enc_den: PCIe gen3-5 use 128b/130b. Gen1/2 used 8b/10b. UCIe and
 * parallel interposer links are unencoded at this layer. */
struct LinkSpec {
    double rate_gt_s;     // per lane
    int    enc_num;
    int    enc_den;
    int    default_lanes;
    bool   serial;        // maps to CACTI-IO Serial vs Low_Swing_Diff
    const char* note;
};

bool specFor(const std::string& t, LinkSpec& s) {
    if (t.rfind("pcie_gen3", 0) == 0) { s = {8.0,  128, 130, 16, true,  "PCIe 3.0: 8 GT/s, 128b/130b"};  return true; }
    if (t.rfind("pcie_gen4", 0) == 0) { s = {16.0, 128, 130, 16, true,  "PCIe 4.0: 16 GT/s, 128b/130b"}; return true; }
    if (t.rfind("pcie_gen5", 0) == 0) { s = {32.0, 128, 130, 16, true,  "PCIe 5.0: 32 GT/s, 128b/130b"}; return true; }
    /* CXL rides the PCIe gen5 electrical layer -- same rate and encoding, so it
     * is DERIVED from gen5 rather than restated, and the two cannot drift. */
    if (t.rfind("cxl", 0) == 0)       { s = {32.0, 128, 130, 16, true,  "CXL on PCIe gen5 electrical"}; return true; }
    /* UCIe advanced package: parallel, unencoded, 64 lanes per module. */
    if (t.rfind("interposer", 0) == 0){ s = {16.0, 1, 1, 64, false, "UCIe advanced package module"}; return true; }
    if (t.rfind("nvlink", 0) == 0)    { s = {25.0, 1, 1, 8,  true,  "NVLink GRS-class per-pin rate"}; return true; }
    if (t.rfind("ualink", 0) == 0)    { s = {200.0, 1, 1, 4, true,  "UALink 200G per lane"}; return true; }
    return false;
}

/* Suppress CACTI-IO's printf reporting. It prints every result to stdout,
 * which is fine for a standalone tool and unacceptable inside a power report.
 * Redirect stdout around the call rather than editing the upstream prints. */
class StdoutSilencer {
public:
    StdoutSilencer() {
        fflush(stdout);
        saved_ = dup(1);
        FILE* null_fp = fopen("/dev/null", "w");
        if (null_fp) { dup2(fileno(null_fp), 1); null_ = null_fp; }
    }
    ~StdoutSilencer() {
        fflush(stdout);
        if (saved_ >= 0) { dup2(saved_, 1); close(saved_); }
        if (null_) fclose(null_);
    }
private:
    int saved_ = -1;
    FILE* null_ = nullptr;
};

}  // namespace

double CactiIOWrapper::linkRateGTs(const std::string& link_type) {
    LinkSpec s;
    return specFor(link_type, s) ? s.rate_gt_s : -1.0;
}

double CactiIOWrapper::linkEncodingEfficiency(const std::string& link_type) {
    LinkSpec s;
    if (!specFor(link_type, s)) return -1.0;
    return static_cast<double>(s.enc_num) / static_cast<double>(s.enc_den);
}

double CactiIOWrapper::linkControllerClockMHz(const std::string& link_type) {
    LinkSpec s;
    if (!specFor(link_type, s)) return -1.0;
    /* PIPE applies to the PCIe electrical family only. NVLink, UALink and
     * UCIe have their own (unpublished-here) controller clocking; refusing
     * beats a 1000 MHz default that silently scales their controller power. */
    const bool pipe_family = (link_type.rfind("pcie_gen", 0) == 0) ||
                             (link_type.rfind("cxl", 0) == 0);
    if (!pipe_family) return -1.0;
    const double kPipeWidthBits = 32.0;   // stated convention, PIPE rev 7.1
    return s.rate_gt_s * 1000.0 / kPipeWidthBits;
}

double CactiIOWrapper::linkBandwidthGBs(const std::string& link_type,
                                        int num_lanes) {
    LinkSpec s;
    if (!specFor(link_type, s)) return -1.0;
    int lanes = (num_lanes > 0) ? num_lanes : s.default_lanes;
    /* GT/s is transfers per second; encoding efficiency converts to payload
     * bits; /8 gives bytes. This is the whole of what 63.0 used to assert:
     * gen5 x16 -> 32 * 16 * (128/130) / 8 = 63.0 GB/s exactly. */
    return s.rate_gt_s * lanes * linkEncodingEfficiency(link_type) / 8.0;
}

/* 1.11.57 (latent C024, and C009 with it): CactiIOWrapper::computeLink() USED
 * TO BE DEFINED HERE, and it is deleted rather than repaired.
 *
 * WHAT WAS WRONG. Two defects sat on top of each other. C024: the fit-range
 * guard a few lines into the body (`freq_mhz > 8000.0` REFUSE) admitted
 * exactly ONE of the seven link types specFor() knows -- pcie_gen3's 8.0 GT/s
 * becomes 8000 MHz and lands on the ceiling exactly, while gen4 (16000),
 * gen5/cxl (32000), interposer (16000), nvlink (25000) and ualink (200000)
 * were all refused. And the single case that survived is the one the header
 * records as returning a termination power of 3.97e+280 mW out of
 * uninitialised memory -- reported with `valid = true` and an
 * `energy_pj_per_bit` computed from it. C009: the body asserted
 * ron_value = 50 and rtt_value = 50 ohm as "specification facts about the
 * electrical layer" while naming no specification, in a file whose DRAM half
 * cites JESD79-3D, JESD8-24 and JESD8-21C by clause. There is no PCIe
 * electrical document in this tree to cite (misc/ holds the JEDEC set and no
 * PCI-SIG material), and the only companion 50 is upstream's own
 * untraceable default at extio_technology.cc:874. The Serial branch also
 * hardcodes rtt1_dq_read = 50 and never reads g_ip->rtt_value, so the rtt
 * half of the claim did nothing even when the code ran.
 *
 * WHY IT WAS INVISIBLE. Nothing called it. A tree-wide grep over src/,
 * include/ and tools/ finds the definition and the declaration and no third
 * site; the only CactiIOWrapper entry points with live callers are
 * linkControllerClockMHz() (main.cpp) and computeDramIO()/dramRateMTs()/
 * dramChannelWidthBits() (src/memory/ramulator_wrapper.cpp). A number nothing
 * asks for cannot reach a report -- which is precisely why both defects could
 * sit here compiled and documented. Deleting is the fix that makes the trap
 * unrepeatable: repairing the prose would have left a one-link entry point
 * whose one answer is uninitialised memory, waiting for a caller.
 *
 * linkBandwidthGBs() and linkRateGTs() are equally uncalled and are KEPT:
 * they return specification primitives with no model behind them, so an
 * unused lookup is not a trap. */

/* ---- DRAM interface: CACTI-IO's validated range ------------------------- */

namespace {

/* Map a PIMID memory technology to a CACTI-IO Mem_IO_type, and say how good
 * the map is. CACTI-IO predates DDR5, LPDDR5, GDDR6 and HBM, so several of
 * these are NEAREST-NEIGHBOUR approximations. Each one is named in the result's
 * `source` string, because an approximation a caller cannot see is the same
 * defect as a constant a caller cannot see. */
struct DramIOMap {
    Mem_IO_type type;
    bool exact;
    const char* note;
};

/* PIMID'S OWN SOURCED ELECTRICAL LAYER, injected into CACTI-IO.
 *
 * CACTI-IO carries one electrical set per io_type, so DDR5/LPDDR5/GDDR6/HBM had
 * to borrow a neighbour's rail voltage and termination -- which is why they were
 * cross-check-only. PIMID already holds these values, sourced, in
 * pimid_energy.h: VDDQ, RTT and RON cited to JESD79-3-1 + T38/T41 (DDR3L
 * SSTL-135, RZQ=240 so RTT=RZQ/6, RON=RZQ/7 -- 1.11.57 audit C008: this line
 * said SSTL-15, naming the 1.5 V part while pimid_energy.h prices the 1.35 V
 * one), JESD8-24 POD12 (DDR4), POD11 (DDR5),
 * JESD8-21C POD135 (GDDR6, RTT programmable via MR6), and the Micron LPDDR5
 * datasheets (LVSTL, VDDQ 0.5 V ODT-on, RON 40).
 *
 * Injecting them and re-deriving the swings is strictly better than borrowing:
 * LPDDR5's 0.5 V rail against LPDDR2's enters every swing as V^2, which is the
 * single biggest error in the borrowed configuration.
 *
 * WHAT IS STILL BORROWED, and must stay stated: capacitances, bias and leakage
 * currents, PHY coefficients and the area polynomial remain CACTI-IO's family
 * values. PIMID has no sourced replacement for those. So an injected technology
 * is better-grounded than a borrowed one but is not fully sourced, and it says
 * so in `source`.
 *
 * RTT for LPDDR5 is the one UNSOURCED input, flagged the same way pimid_energy.h
 * flags it: Micron defers the ohm table to a separate AC/DC document we do not
 * have, so 240 (RZQ, the LPDDR4/5 ODT reference) stands in. */
/* 1.11.57 (latent C009): the `mts` FIELD IS GONE.
 *
 * It was a fourth transfer-rate table in a tree that has three too many
 * already, and it had gone stale in exactly the way the others did: DDR4 held
 * 3200, the literal 1.11.52 (D002) removed from dramRateMTs() and from the
 * energy path because this tree simulates DDR4-2400, and DDR5 held 4800, the
 * one 1.11.57 (C001) removed twenty lines below. Nothing read it -- every
 * consumer takes the rate as the `rate_mts` argument -- so nothing reachable
 * differed, which is the only reason two retired speed bins survived here.
 * Deleted rather than corrected: the rate has one authority in this file and
 * this struct is not it. What belongs here is what the name says, the
 * ELECTRICALS: the rail, the termination and the driver impedance. */
struct PimidElectrical {
    double vddq;      // IO rail -> vdd_io
    double rtt;       // termination -> rtt1_dq_*
    double ron;       // driver on-resistance -> r_on
    bool   sourced;   // is every electrical value cited?
    const char* note;
};

bool pimidElectricalFor(const std::string& t, PimidElectrical& e) {
    /* 1.11.57 (audit C008): 1.35 V, not 1.5 V -- the DDR3L part the rest of
     * this run prices. The IDD row that gives DDR3 its ARRAY energy
     * (external/ramulator/src/dram/pimid_energy.h) is Micron 4Gb DDR3L-1600, a
     * 1.35 V part, and 1.11.46 retired the 1.5 V SSTL-15 reading there under
     * the heading "ONE PART per technology". That ruling was applied to the
     * fallback scheme table and not to this one -- which OVERRIDES it, since
     * DDR3's CACTI-IO map is exact and the injected electricals make the result
     * authoritative for the printed termination energy. So one DDR3 access was
     * priced on two different silicon voltages, and the half that reached the
     * report was the retired one. The swing voltages recomputeSwing() derives
     * scale with this rail and the termination power with its square. */
    if (t == "DDR3")   { e = {1.35, 40,  34, true,  "SSTL-135, JESD79-3-1 + T38/T41 (RZQ=240); DDR3L, the part the IDD row describes"}; return true; }
    if (t == "DDR4")   { e = {1.2,  48,  40, true,  "POD12, JESD8-24"};                            return true; }
    if (t == "DDR5")   { e = {1.1,  48,  40, true,  "POD11, same family as JESD8-24"};             return true; }
    if (t == "GDDR6")  { e = {1.35, 60,  40, true,  "POD135, JESD8-21C Cl.D (RTT via MR6)"};       return true; }
    if (t == "LPDDR5") { e = {0.5,  240, 40, false, "LVSTL, Micron LPDDR5 datasheets; RTT=240 (RZQ) UNSOURCED"}; return true; }
    /* HBM rides an interposer: JESD238B cl.9.1, unterminated, so there is no
     * termination network to inject. It keeps WideIO's low-swing electricals,
     * which is the physically right family for a wide unterminated bus. */
    return false;
}

bool dramIOMapFor(const std::string& t, DramIOMap& m) {
    if (t == "DDR3")   { m = {DDR3,   true,  "DDR3, exact"}; return true; }
    if (t == "DDR4")   { m = {DDR4,   true,  "DDR4, exact"}; return true; }
    /* DDR5 postdates CACTI-IO. DDR4 is the nearest: same POD-style signalling
     * family and on-die termination, lower VDDQ (1.1 vs 1.2 V) and higher
     * rates. APPROXIMATE -- termination topology matches, voltage does not. */
    if (t == "DDR5")   { m = {DDR4,   false, "DDR5 -> DDR4 (POD family matches; VDDQ 1.1 vs 1.2 V unmodelled)"}; return true; }
    if (t == "LPDDR5") { m = {LPDDR2, false, "LPDDR5 -> LPDDR2 (low-power family; LVSTL ground-referenced termination unmodelled)"}; return true; }
    /* HBM is a wide, slow, low-swing parallel interface on an interposer --
     * which is exactly what WideIO models. The closest map in the set. */
    if (t == "HBM2" || t == "HBM3")
                       { m = {WideIO, false, "HBM -> WideIO (wide low-swing parallel on interposer; per-channel stack structure unmodelled)"}; return true; }
    /* GDDR6: POD135, same signalling family as DDR4/DDR5, so DDR4's structural
     * and PHY coefficients apply once PIMID's POD135 electricals are injected.
     *
     * 1.11.52 (audit C021): the previous note claimed the 14 Gb/s rate "will
     * be refused" by the fit-range check. It is not: the check is on the BUS
     * CLOCK (rate/2 = 7000 MHz) against a 8000 MHz ceiling, so GDDR6 passes
     * and is fully modelled. The claim was written when the ceiling was
     * compared against the transfer rate. It is modelled, not refused -- and
     * its command-bus structure is given explicitly below rather than
     * falling into the DDR default. */
    if (t == "GDDR6")  { m = {DDR4, false, "GDDR6 -> DDR4 structure + PIMID POD135 electricals"}; return true; }
    return false;
}

}  // namespace

/* DRAM specification primitives. Rates are the standard's data rates; widths
 * are the per-channel data-bus width the standard defines. Single values that
 * a specification fixes exactly -- the only kind that belongs in a table. */
double CactiIOWrapper::dramRateMTs(const std::string& t) {
    if (t == "DDR3")   return 1600;
    /* 1.11.52 (audit D002): the rate of the part THIS TREE SIMULATES. DDR4
     * ran as DDR4-2400 everywhere that decides timing -- the Ramulator
     * preset (DDR4_2400R), the architecture object, and the wrapper's own
     * 19.2 GB/s -- while this table said 3200, so one access had its array
     * half priced at 2400 and its DQ-termination half at 3200 (termination
     * 1.33x understated). POD12 is an interface standard, not a speed bin,
     * so the electricals are unchanged by this. */
    if (t == "DDR4")   return 2400;
    /* 1.11.57 (audit round 3, C001): 3200, not 4800. The 1.11.56 speed-bin
     * work changed the DRAM ARCHITECTURE OBJECTS to name the preset this tree
     * simulates and missed this table -- which sits directly under the D002
     * comment stating that exact principle. DDR5's electrical map is
     * injected+sourced, so exact_map is true and CACTI-IO's figure REPLACES
     * the scheme table: the termination energy was quoted for a part 1.5x
     * faster than the one whose cycles are counted. */
    if (t == "DDR5")   return 3200;   // preset DDR5_3200AN
    if (t == "LPDDR5") return 6400;
    if (t == "GDDR6")  return 14000;
    if (t == "HBM2")   return 2400;   // 1.11.57 (C001): preset HBM2_2.4Gbps
    if (t == "HBM3")   return 6400;
    return -1.0;
}

int CactiIOWrapper::dramChannelWidthBits(const std::string& t) {
    if (t == "DDR3" || t == "DDR4" || t == "DDR5") return 64;   // 64-bit channel
    if (t == "LPDDR5") return 16;                               // 16-bit channel
    if (t == "GDDR6")  return 32;                               // 32-bit, 2x16 pseudo
    if (t == "HBM2" || t == "HBM3") return 128;                 // 128-bit channel
    return -1;
}

LinkIOResult CactiIOWrapper::computeDramIO(const std::string& tech,
                                           int num_dq,
                                           double rate_mts,
                                           double activity) {
    LinkIOResult r;
    DramIOMap m;
    if (!dramIOMapFor(tech, m)) {
        r.source = "no CACTI-IO parameter set for memory technology '" + tech +
                   "' -- refusing rather than substituting a neighbour";
        return r;
    }
    /* 1.11.57 (latent C019): these two defaults ANNOUNCE themselves now.
     *
     * What was wrong: a caller that handed in a non-positive channel width or
     * transfer rate had it silently replaced by a 64-bit DDR-class channel at
     * 3200 MT/s, and the result came back with valid = true, exact_map set
     * from the technology's own map, and a `source` string naming the
     * technology the caller asked about. An HBM3 request with a missing rate
     * would have been answered as if it were DDR4-3200 -- and every consumer
     * of this result (termination energy, interface dynamic energy, IO area,
     * and the pJ/bit derived from all three) would have been priced off a
     * geometry nobody chose. num_dq is the channel's DATA-BUS WIDTH: it sets
     * the DQ pin count, and through the n_dqs/n_ca derivation below it also
     * sets the strobe and command-bus pin counts, so it governs every
     * per-pin term in the model as well as the denominator of the pJ/bit.
     * rate_mts is the transfer rate: bus_freq is rate_mts/2, which governs
     * dynamic power, the PHY terms, and which side of the 3162 MHz area
     * crossover and the 8000 MHz fit ceiling the evaluation falls on.
     *
     * Why it was invisible: no reachable caller can trigger it. All three
     * computeDramIO() call sites (src/memory/ramulator_wrapper.cpp) build
     * their arguments from dramRateMTs()/dramChannelWidthBits() above, both
     * of which return the standard's value for every technology they accept
     * and a negative for the rest, and then guard on them (e.g. `if (rate <=
     * 0.0 || ndq <= 0) return 0.0;`) before calling. So the substitution has
     * never fired. It stays as a guard rather than becoming a refusal because
     * dividing by a zero width downstream is worse than a stated default --
     * but it no longer passes silently, and the result says so too. */
    std::string substituted_geometry;   // 1.11.57 (C019): carried into r.source
    if (num_dq <= 0) {
        substituted_geometry += "; DATA-BUS WIDTH SUBSTITUTED (caller gave a "
                                "non-positive num_dq; priced as a 64-bit "
                                "DDR-class channel)";
        std::cerr << "[power] WARNING: computeDramIO(" << tech
                  << ") was given num_dq=" << num_dq
                  << "; SUBSTITUTING a 64-bit DDR-class data bus. This governs "
                     "the DQ, strobe and command pin counts and the per-bit "
                     "denominator -- the result below is for a 64-bit channel, "
                     "not for whatever the caller has." << std::endl;
        num_dq = 64;      // one DDR-class channel
    }
    if (rate_mts <= 0) {
        substituted_geometry += "; TRANSFER RATE SUBSTITUTED (caller gave a "
                                "non-positive rate_mts; priced at 3200 MT/s)";
        std::cerr << "[power] WARNING: computeDramIO(" << tech
                  << ") was given rate_mts=" << rate_mts
                  << "; SUBSTITUTING 3200 MT/s (DDR4-3200). This governs "
                     "bus_freq, every dynamic and PHY term, and which side of "
                     "the area/fit ceilings the evaluation lands on -- the "
                     "result below is for a 3200 MT/s part." << std::endl;
        rate_mts = 3200;
    }
    if (activity < 0.0) activity = 0.0;
    if (activity > 1.0) activity = 1.0;

    /* 1.11.45 (E30): POINTER-SWAP, never a value copy. Point the global at
     * our OWN object for the duration of the call and restore the previous
     * POINTER on every exit path, dereferencing nothing we do not own. The
     * retired form saved *g_ip and later wrote *g_ip = saved, which is a
     * write THROUGH whatever the global points at -- freed memory after any
     * CACTIWrapper's death, and a dangling pointer is not null, so a null
     * check cannot save you.
     * 1.11.57 (latent C024): this used to read "same rule as computeLink
     * above" and computeLink is gone, so the rule is stated here instead of
     * pointing at a deleted function. */
    static InputParameter s_dram_ip;
    InputParameter* prev_gip = g_ip;
    s_dram_ip = InputParameter();
    g_ip = &s_dram_ip;

    /* CHANNEL STRUCTURE -- per technology, because it is not shared.
     * A first cut applied DDR4's structure (25 CA pins, a strobe pair per
     * byte) to everything. On a 16-bit LPDDR5 channel that makes the command
     * bus WIDER than the data bus, and the fixed CA and PHY cost is then
     * amortised over 16 lanes instead of 64 -- LPDDR5 came out at ~71 pJ/bit
     * against a real ~3-6. The pin counts below are what each standard
     * actually defines, which is the only reason they are constants.
     *
     *   DDR3/4/5  64 DQ, one differential strobe pair per byte (16), a shared
     *             ~25-pin command/address bus, one differential clock.
     *   LPDDR5    16 DQ per channel, a 7-pin CA bus (LPDDR5 command encoding),
     *             WCK/RDQS pairs per byte, one differential clock.
     *   HBM2/3    128 DQ per channel, strobe per byte, split row/column
     *             command buses (~14 pins), one differential clock. Wide and
     *             slow, which is why its per-bit energy is the lowest here.
     *   GDDR6     32 DQ per channel (2x16 pseudo-channels), WCK/EDC per
     *             byte, and a ~12-pin single-ended CA bus (JESD250: CA0-9
     *             plus CABI/CKE-class pins) -- NOT the 25-pin DDR-class
     *             command bus. 1.11.52 (audit C021): GDDR6 used to fall into
     *             the DDR default below, so a 25-pin CA bus was amortised
     *             over 32 data lanes -- the same fixed-CA-over-few-lanes
     *             shape the LPDDR5 note above records as producing ~71
     *             pJ/bit against a real 3-6. */
    int n_dqs, n_ca;
    if (tech == "LPDDR5") {
        n_dqs = (num_dq / 8) * 2;   // WCK/RDQS pair per byte
        n_ca  = 7;                  // LPDDR5 CA bus
    } else if (tech == "GDDR6") {
        n_dqs = (num_dq / 8) * 2;   // WCK + EDC per byte
        n_ca  = 12;                 // JESD250 single-ended CA bus
    } else if (tech == "HBM2" || tech == "HBM3") {
        n_dqs = (num_dq / 8) * 2;
        n_ca  = 14;                 // row + column command buses
    } else {
        n_dqs = (num_dq / 8) * 2;
        n_ca  = 25;                 // DDR-class command/address
    }
    g_ip->io_type   = m.type;
    g_ip->num_dq    = num_dq;
    g_ip->num_dqs   = n_dqs;
    g_ip->num_ca    = n_ca;
    g_ip->num_clk   = 2;
    g_ip->num_mem_dq     = 8;
    g_ip->mem_data_width = 8;
    g_ip->duty_cycle  = activity;
    g_ip->activity_dq = activity;
    g_ip->activity_ca = activity;
    g_ip->iostate     = WRITE;
    g_ip->bus_freq    = rate_mts / 2.0;   // MT/s is DDR: two transfers per clock
    g_ip->dram_dimm   = UDIMM;
    g_ip->num_bobs    = 1;
    g_ip->num_channels_per_bob = 1;
    if (g_ip->capacity <= 0) g_ip->capacity = 8;
    /* Driver and termination impedances: CACTI-IO reads these from g_ip and
     * upstream marks them //FIXME. These are the JEDEC ODT settings for a
     * DDR4 UDIMM: 34 ohm driver, 60 ohm termination, 0.5 ns flight time.
     *
     * 1.11.56 (audit C023): the guards are GONE and the comment no longer
     * claims they are conditional. It said "used only when the caller left
     * them unset", which was true of nothing: computeDramIO()'s signature has
     * no path for a caller to set them, and s_dram_ip is reset to a
     * default-constructed InputParameter at the top of this function (io.cc's
     * constructor zeroes all three), so `if (<= 0.0)` was always true and the
     * three assignments always fired. Writing them plainly is the same
     * behaviour with an honest description.
     *
     * WHERE THEY ACTUALLY LAND, which the old wording also obscured: only
     * the DDR3/DDR4 parameter sets read g_ip for these, so DDR3, DDR4, DDR5
     * and GDDR6 are priced with a 34 ohm driver, a 60 ohm second termination
     * and a 0.5 ns flight time in every run. The LPDDR2 and WideIO branches
     * (extio_technology.cc) hardcode their own and never look at g_ip, so
     * for LPDDR5 and HBM2/HBM3 these three lines do nothing at all. */
    g_ip->ron_value     = 34.0;
    g_ip->rtt_value     = 60.0;
    g_ip->tflight_value = 0.5;

    double freq_mhz = g_ip->bus_freq;
    const double kFitMaxMHz = 8000.0;
    if (freq_mhz > kFitMaxMHz) {
        g_ip = prev_gip;
        r.source = "rate beyond CACTI-IO's fitted range; refused";
        return r;
    }

    try {
        IOTechParam iop(g_ip, m.type, 8, 8, num_dq, 0, 1, freq_mhz);
        /* INJECT PIMID'S SOURCED ELECTRICALS and re-derive. This is what lets a
         * technology CACTI-IO predates be modelled rather than merely borrowed:
         * the rail voltage, termination and driver impedance come from JEDEC and
         * the vendor datasheets, and recomputeSwing() propagates them through the
         * termination network into every swing the power model reads. */
        PimidElectrical el;
        bool injected = pimidElectricalFor(tech, el);
        if (injected) {
            iop.vdd_io        = el.vddq;
            iop.rtt1_dq_read  = el.rtt;
            iop.rtt1_dq_write = el.rtt;
            iop.r_on          = el.ron;
            /* 1.11.52 (audit C022): the injection is PARTIAL, and the
             * uninjected legs are load-bearing -- extio.cc's read/write
             * termination sums 1/rtt1 + 1/rtt2 and its command-bus term uses
             * r_on_ca + rtt_ca. rtt2 is the FAR-END termination of the same
             * DQ net, so for the point-to-point topologies we model it is
             * the same device parameter as rtt1 and is injected with it;
             * rs1/rs2 (series resistors), rtt_ca and z0 stay at the borrowed
             * family's values because our tables carry no per-technology
             * source for them. The `source` string below names the split so
             * a reader is not told "ELECTRICALS INJECTED" about a result
             * that is partly the neighbour's. */
            iop.rtt2_dq_read  = el.rtt;
            iop.rtt2_dq_write = el.rtt;
            iop.recomputeSwing();
        }
        Extio io(&iop);
        {
            StdoutSilencer quiet;
            io.extio_area();
            io.extio_power_term();
            io.extio_power_phy();
            io.extio_power_dynamic();
            io.extio_eye();
        }
        /* AREA HAS A NARROWER VALID RANGE THAN POWER, and they must not share
         * one limit. Power scales linearly in frequency; the AREA polynomial's
         * cubic term equals its linear term at sqrt(k1/k3) = 3162 MHz and is
         * 4.9x it by 7000 MHz (GDDR6's clock), which is what produced an
         * implausible 10.8 mm^2 for a 32-bit GDDR6 interface. Above the
         * crossover the area is an extrapolation dominated by the cubic, so it
         * is withheld rather than reported -- power stays, because nothing in
         * the power path depends on that polynomial. */
        const double kAreaValidMaxMHz = 3162.0;   // cubic == linear
        const bool area_credible = (freq_mhz <= kAreaValidMaxMHz);
        r.io_area_mm2          = area_credible ? io.getIOAreaMM2() : 0.0;
        r.io_power_term_mw     = io.getIOPowerTermMW();
        r.io_power_dynamic_mw  = io.getIOPowerDynamicMW();
        r.phy_power_mw         = io.getPHYPowerMW();
        r.phy_static_power_mw  = io.getPHYStaticPowerMW();
        r.phy_dynamic_power_mw = io.getPHYDynamicPowerMW();
        r.timing_margin_ui     = io.getTimingMarginUI();
        r.voltage_margin_v     = io.getVoltageMarginV();
        double payload_gbps = rate_mts * num_dq / 1000.0;
        double total_mw = r.io_power_term_mw + r.io_power_dynamic_mw + r.phy_power_mw;
        if (payload_gbps > 0.0) {
            r.energy_pj_per_bit = total_mw / payload_gbps;
            r.energy_pj_per_bit_term = r.io_power_term_mw / payload_gbps;
        }
        r.valid  = true;
        /* SUBSTITUTION GATE. An injected technology is no longer merely
         * borrowed -- its electrical layer comes from JEDEC and the vendor
         * datasheets, and only the capacitance/PHY/area coefficients come
         * from the neighbouring family -- so it may REPLACE the scheme-table
         * number. A map with neither an exact set nor an injection stays
         * cross-check-only.
         *
         * 1.11.52 (audit C020): the injection must be FULLY SOURCED to
         * substitute. The premise of the paragraph above is "its electrical
         * layer is sourced", and for LPDDR5 that is false by our own record:
         * its RTT = 240 ohm is flagged UNSOURCED in the same table (Micron
         * gives VDDQ and RON, not Rtt). Letting it substitute meant an
         * assumption silently replaced the termination energy, the interface
         * dynamic energy and the IO area -- exactly the substitution the
         * consumer's own comment says these technologies do not get. An
         * unsourced injection is still computed and REPORTED, as a
         * cross-check; it just does not replace. */
        r.exact_map = m.exact || (injected && el.sourced);
        if (injected && !el.sourced) {
            std::cerr << "[power] NOTE: " << tech
                      << " CACTI-IO result is CROSS-CHECK ONLY: its injected "
                         "electricals contain an unsourced value (" << el.note
                      << "), so it does not replace the scheme-table "
                         "termination." << std::endl;
        }
        r.source = std::string("CACTI-IO ")
                 + (m.exact ? "" : "structure from " + std::string(m.note) + "; ")
                 + (injected
                      ? std::string("ELECTRICALS PARTIALLY INJECTED from PIMID "
                                    "(vddq, rtt1/rtt2 DQ, r_on; rs1/rs2, "
                                    "rtt_ca and z0 remain the borrowed "
                                    "family's): ") + el.note
                        + (el.sourced ? "" : " [contains an UNSOURCED value]")
                      : std::string("family electricals"))
                 + substituted_geometry;   /* 1.11.57 (latent C019) */
        r.not_modelled = std::string(
            area_credible ? "" : "IO AREA WITHHELD: above 3162 MHz the area "
                                 "polynomial's cubic term exceeds its linear "
                                 "term and the value is an extrapolation; ")
            + std::string("refresh, array access and controller logic -- IO only")
            + (injected
                 ? "; capacitances, bias/leak currents, PHY coefficients and the "
                   "area polynomial are still the neighbouring family's, not sourced"
                 : "");
    } catch (...) {
        r.valid = false;
        r.source = "CACTI-IO threw during evaluation";
    }
    g_ip = prev_gip;
    return r;
}

}  // namespace PIMID
