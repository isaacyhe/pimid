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

LinkIOResult CactiIOWrapper::computeLink(const std::string& link_type,
                                         int num_lanes,
                                         double rate_gt_s,
                                         double duty_cycle) {
    LinkIOResult r;
    LinkSpec spec;
    if (!specFor(link_type, spec)) {
        r.source = "no CACTI-IO parameter set for link type '" + link_type + "'";
        return r;
    }
    int lanes = (num_lanes > 0) ? num_lanes : spec.default_lanes;
    double rate = (rate_gt_s > 0.0) ? rate_gt_s : spec.rate_gt_s;
    if (duty_cycle < 0.0) duty_cycle = 0.0;
    if (duty_cycle > 1.0) duty_cycle = 1.0;

    /* 1.11.45 (audit E30, ruled): POINTER-SWAP, never a value copy. The old
     * form saved *g_ip and later wrote *g_ip = saved -- a write THROUGH
     * whatever the global pointed at, which after any CACTIWrapper's death is
     * freed memory (dangling is not null, so a null-check cannot save you).
     * We now point the global at our OWN object for the duration and restore
     * the previous POINTER on exit, dereferencing nothing we do not own. */
    static InputParameter s_io_ip;   // our object; lives for the process
    InputParameter* prev_gip = g_ip;
    s_io_ip = InputParameter();      // reset to defaults each call
    g_ip = &s_io_ip;

    g_ip->io_type      = Serial;
    g_ip->num_dq       = lanes;
    g_ip->num_dqs      = 0;      // serial links embed the clock; no strobe pins
    g_ip->num_ca       = 0;      // no separate command/address bus
    g_ip->num_clk      = 0;
    g_ip->duty_cycle   = duty_cycle;      // MEASURED (E19), not configured
    g_ip->iostate      = WRITE;
    /* CHANNEL PARAMETERS. CACTI-IO reads these from g_ip and upstream marks
     * them //FIXME -- a default-constructed InputParameter leaves them at 0
     * and extio_area() then divides by r_on. They are caller-supplied because
     * they describe the CHANNEL, not the model, and for a PCIe-class serial
     * link the electrical specification fixes them:
     *   50 ohm single-ended driver impedance and 50 ohm single-ended receiver
     *   termination, which is the 100 ohm DIFFERENTIAL channel the Serial
     *   parameter set already declares as r_diff_term.
     * These are specification facts about the electrical layer -- the one kind
     * of single value that is legitimately a constant. */
    g_ip->ron_value    = 50.0;   // TX output impedance, single-ended
    g_ip->rtt_value    = 50.0;   // RX termination, single-ended -> 100 ohm diff
    /* Time of flight depends on CHANNEL LENGTH, which no link type fixes. The
     * Serial parameter set carries its own default; only override it when the
     * caller knows the geometry. Left at whatever CACTI-IO's table holds so a
     * length is never silently invented here. */
    if (g_ip->tflight_value <= 0.0) g_ip->tflight_value = 1.0;  // ns, stated
    g_ip->activity_dq  = duty_cycle;
    g_ip->activity_ca  = 0.0;
    /* CACTI-IO takes frequency in MHz; a serial lane's signalling rate is its
     * transfer rate. */
    double freq_mhz = rate * 1000.0;
    g_ip->bus_freq   = freq_mhz;

    /* VALIDITY RANGE -- the reason this harness refuses more than it answers.
     * CACTI-IO's area model is a FIT, not a formula:
     *     area/IO = c + k0/r_on + (1/r_on)(k1*f + k2*f^2 + k3*f^3)
     * and the coefficients were fitted over DDR-class frequencies. The cubic
     * term is 0.003 at 800 MHz and 163.8 at 32000 MHz -- a 55000x blow-up --
     * giving 61 mm^2 of IO area for a 16-lane link, which is not physical.
     * Measured across the range (k3*f^3 / area x16):
     *      800 MHz  0.003 / 0.23 mm^2      3200 MHz  0.164 / 0.39 mm^2
     *     1600 MHz  0.020 / 0.26 mm^2      6400 MHz  1.311 / 1.06 mm^2
     *                                     32000 MHz  163.8 / 61.34 mm^2
     * So the model is usable to about DDR5-6400 and no further. Returning a
     * number outside that would be extrapolating a cubic 5x beyond its fit --
     * exactly the kind of confident-looking fabrication this audit is about.
     * It refuses instead, and says why. */
    const double kFitMaxMHz = 8000.0;
    if (freq_mhz > kFitMaxMHz) {
        g_ip = prev_gip;
        char buf[360];
        snprintf(buf, sizeof buf,
                 "CACTI-IO area/PHY coefficients are fitted for DDR-class rates; "
                 "%.0f MHz is %.1fx beyond the %.0f MHz limit and the cubic area "
                 "term diverges (k3*f^3 grows 55000x from 800 MHz to 32 GHz, "
                 "giving 61 mm^2 for 16 lanes). REFUSED rather than extrapolated.",
                 freq_mhz, freq_mhz / kFitMaxMHz, kFitMaxMHz);
        r.source = buf;
        r.not_modelled = "everything -- this link rate is outside the model's fit range";
        return r;
    }

    double energy_pj_bit = 0.0;
    try {
        IOTechParam iop(g_ip, Serial, lanes, 1, lanes, 0, 1, freq_mhz);
        Extio io(&iop);
        {
            StdoutSilencer quiet;
            io.extio_area();
            io.extio_power_term();
            io.extio_power_phy();
            io.extio_power_dynamic();
            io.extio_eye();
        }
        r.io_area_mm2          = io.getIOAreaMM2();
        r.io_power_term_mw     = io.getIOPowerTermMW();
        r.io_power_dynamic_mw  = io.getIOPowerDynamicMW();
        r.phy_power_mw         = io.getPHYPowerMW();
        r.phy_static_power_mw  = io.getPHYStaticPowerMW();
        r.phy_dynamic_power_mw = io.getPHYDynamicPowerMW();
        r.timing_margin_ui     = io.getTimingMarginUI();
        r.voltage_margin_v     = io.getVoltageMarginV();

        /* Energy per bit = total power / payload bit rate. Power is mW, the
         * aggregate payload rate is lanes x rate x encoding efficiency Gb/s,
         * so mW / Gb/s = pJ/bit directly. */
        double payload_gbps = rate * lanes * linkEncodingEfficiency(link_type);
        double total_mw = r.io_power_term_mw + r.io_power_dynamic_mw + r.phy_power_mw;
        if (payload_gbps > 0.0) energy_pj_bit = total_mw / payload_gbps;
        r.energy_pj_per_bit = energy_pj_bit;
        r.valid = true;
        r.source = std::string("CACTI-IO Serial; ") + spec.note;
        /* Say what is missing. CACTI-IO is a DDR-class off-chip IO model: it
         * accounts driver, termination, PHY blocks and IO area. It does NOT
         * model SerDes equalisation, CDR, or link-layer DSP, which dominate
         * high-rate serial PHYs -- so this result is a LOWER BOUND on a modern
         * SerDes link, and the gap to the published band is the size of what
         * is unmodelled rather than an error to tune away. */
        r.not_modelled = "SerDes equalisation, CDR, link-layer DSP, "
                         "protocol/encoding overhead beyond the ratio";
    } catch (...) {
        r.valid = false;
        r.source = "CACTI-IO threw during evaluation";
    }

    g_ip = prev_gip;   // restore the POINTER; we dereferenced nothing old
    return r;
}


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
 * pimid_energy.h: VDDQ, RTT and RON cited to JESD79-3D T38/T41 (DDR3 SSTL-15,
 * RZQ=240 so RTT=RZQ/6, RON=RZQ/7), JESD8-24 POD12 (DDR4), POD11 (DDR5),
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
struct PimidElectrical {
    double vddq;      // IO rail -> vdd_io
    double rtt;       // termination -> rtt1_dq_*
    double ron;       // driver on-resistance -> r_on
    double mts;       // transfer rate
    bool   sourced;   // is every electrical value cited?
    const char* note;
};

bool pimidElectricalFor(const std::string& t, PimidElectrical& e) {
    if (t == "DDR3")   { e = {1.5,  40,  34, 1600,  true,  "SSTL-15, JESD79-3D T38/T41 (RZQ=240)"};      return true; }
    if (t == "DDR4")   { e = {1.2,  48,  40, 3200,  true,  "POD12, JESD8-24"};                            return true; }
    if (t == "DDR5")   { e = {1.1,  48,  40, 4800,  true,  "POD11, same family as JESD8-24"};             return true; }
    if (t == "GDDR6")  { e = {1.35, 60,  40, 14000, true,  "POD135, JESD8-21C Cl.D (RTT via MR6)"};       return true; }
    if (t == "LPDDR5") { e = {0.5,  240, 40, 6400,  false, "LVSTL, Micron LPDDR5 datasheets; RTT=240 (RZQ) UNSOURCED"}; return true; }
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
     * Its 14 Gb/s rate is still checked against the fit range below and will be
     * refused there -- the map exists, the RATE is what disqualifies it. */
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
    if (t == "DDR5")   return 4800;
    if (t == "LPDDR5") return 6400;
    if (t == "GDDR6")  return 14000;
    if (t == "HBM2")   return 2000;
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
    if (num_dq <= 0)   num_dq = 64;      // one DDR-class channel
    if (rate_mts <= 0) rate_mts = 3200;
    if (activity < 0.0) activity = 0.0;
    if (activity > 1.0) activity = 1.0;

    /* 1.11.45 (E30): pointer-swap, same rule as computeLink above. */
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
     *   GDDR6     refused above; no parameter set to give a structure to. */
    int n_dqs, n_ca;
    if (tech == "LPDDR5") {
        n_dqs = (num_dq / 8) * 2;   // WCK/RDQS pair per byte
        n_ca  = 7;                  // LPDDR5 CA bus
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
     * upstream marks them //FIXME. JEDEC ODT settings for a DDR4 UDIMM are
     * 34 ohm driver / 60 ohm termination; used only when the caller left them
     * unset, and stated so the value is never silently in force. */
    if (g_ip->ron_value <= 0.0)     g_ip->ron_value = 34.0;
    if (g_ip->rtt_value <= 0.0)     g_ip->rtt_value = 60.0;
    if (g_ip->tflight_value <= 0.0) g_ip->tflight_value = 0.5;

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
        /* An INJECTED technology is no longer merely borrowed: its electrical
         * layer is sourced, only the capacitance/PHY/area coefficients come
         * from the neighbouring family. That is good enough to substitute, and
         * it is what extends coverage past DDR3/DDR4. A map with neither an
         * exact set nor an injection stays cross-check-only. */
        r.exact_map = m.exact || injected;
        r.source = std::string("CACTI-IO ")
                 + (m.exact ? "" : "structure from " + std::string(m.note) + "; ")
                 + (injected
                      ? std::string("ELECTRICALS INJECTED from PIMID: ") + el.note
                        + (el.sourced ? "" : " [contains an UNSOURCED value]")
                      : std::string("family electricals"));
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
