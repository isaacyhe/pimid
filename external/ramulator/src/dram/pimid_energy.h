#ifndef RAMULATOR_DRAM_PIMID_ENERGY_H
#define RAMULATOR_DRAM_PIMID_ENERGY_H
// ---------------------------------------------------------------------------
// PIMID intensive DRAM energy layer -- RELOCATED into Ramulator2 (1.9.10).
//
// This is the single source of the per-access (intensive) DRAM energy the PIMID
// power path consumes. It lives inside external/ramulator so the physics belongs
// to Ramulator2, not the PIMID wrapper; the wrapper (ramulator_wrapper.cpp) is now
// a thin reader that forwards its timing getters into these functions.
//
// The per-tech IDD/VDD tables here are PART-NUMBER-SOURCED datasheet values
// (Micron part classes named below). 1.11.46 (FIX-PRE-FLEET L189): the claim
// that they "mirror the per-impl current_presets" was FALSE and is withdrawn:
// measured against the tree, DDR4.cpp's Default preset is {60,50,55,145,145,
// IDD5B 362} vs this table's {58,35,42,140,150, IDD5 155} -- the divergence
// lands on the IDD4W write term 1.11.5 introduced, and the upstream DDR5
// preset is a byte-identical copy of DDR4's (generic, not a DDR5 part). The
// upstream presets are unlabelled defaults for the command-driven extensive
// model (drampower_enable, off in our runs); THIS table is the authoritative
// intensive source, and IDD5 here is the average-refresh current, not
// upstream's IDD5B burst figure -- different definitions, not a typo. Ramulator2's own command-driven
// power model (update_powers()/s_total_*energy, gated by drampower_enable) produces
// EXTENSIVE totals over a full simulation; this header produces the INTENSIVE
// per-64B-access / per-device values used for analytical energy accounting. Both
// draw from the same datasheet numbers.
//
// Formulas: Micron TN-41-01 (row ACT+PRE and read-burst energy); ODT/termination =
// resistor-network dissipation VDDQ^2/Rtt over the bit period, per I/O standard.
// ---------------------------------------------------------------------------
#include <string>

namespace Ramulator {
namespace pimid_energy {

struct IDDSpec {
    double vdd;                                    // V
    double idd0, idd2n, idd3n, idd4r, idd4w, idd5; // mA (per device / per channel)
    double trfc_ns, trefi_ns;
    int    channels;                               // per-stack aggregation (HBM)
    /* 1.11.8 (#84): precharge power-down current (CKE low, fast tXP exit) --
     * the JEDEC descent state an idle controller actually enters. Same
     * datasheet classes as the columns above. */
    double idd2p;                                  // mA
};

// Per-tech datasheet/JESD IDD-class values (part-number classes in comments; see
// the assumptions register). Mirrors the per-impl current_presets/voltage_presets.
//   DDR5  Micron 16Gb DDR5-4800 (VDD 1.1);  DDR4 Micron 8Gb DDR4-2400 (1.2);
//   DDR3  Micron 4Gb DDR3L-1600 (1.35);     LPDDR5 Micron 12Gb LPDDR5-6400 (1.05);
//   GDDR6 Micron 8Gb GDDR6-14000 (1.35);    HBM2 JESD235 (1.2); HBM3 JESD238 (1.1).
inline IDDSpec iddFor(const std::string& tech) {
    /* idd2p (last column) from the same part-number classes: DDR5-4800 ~20mA,
     * DDR4-2400 ~25 (IDD2P fast-exit), DDR3L ~18, LPDDR5 ~4 (deep mobile
     * power-down class), GDDR6 ~30, HBM2/3 ~30-40% of IDD2N per JESD
     * precharge-standby-powerdown deltas. */
    if (tech == "DDR5")   return {1.1, 55,34,42,148,168,120, 295.0, 3900.0, 1, 20};
    if (tech == "DDR4")   return {1.2, 58,35,42,140,150,155, 350.0, 7800.0, 1, 25};
    if (tech == "DDR3")   return {1.35,60,32,45,175,180,210, 260.0, 7800.0, 1, 18};
    if (tech == "LPDDR5") return {1.05,32,18,24,110,120, 90, 210.0, 3904.0, 1,  4};
    if (tech == "GDDR6")  return {1.35,70,45,60,210,230,180, 220.0, 1900.0, 1, 30};
    if (tech == "HBM3")   return {1.1, 30,18,22, 90,100, 70, 160.0, 3900.0, 16, 7};
    if (tech == "HBM2")   return {1.2, 28,17,21, 80, 90, 65, 160.0, 3900.0, 8,  7};
    return {1.2, 58,35,42,140,150,155, 350.0, 7800.0, 1, 25};  // unknown -> DDR4 class
}

/* 1.11.46 (FIX-PRE-FLEET L181): DEVICES PER ACCESS. The IDD columns are
 * PER-DEVICE currents (the struct says so), but a 64 B access on a DDR-class
 * 64-bit rank engages EVERY chip in the rank simultaneously -- x8 parts: 8
 * chips each activating and bursting 8 of the 64 DQ lines. Micron TN-41-01,
 * this file's own cited formula source, multiplies per-DRAM power by the
 * number of DRAMs; we did not, so the array terms were per-DEVICE while the
 * termination term (x512 bits) was whole-rank -- the two bases the audit
 * caught being summed. HBM's IDD is per CHANNEL and an access stays in one
 * channel; LPDDR5/GDDR6 are one die per channel. */
inline int devicesPerAccess(const std::string& tech,
                            const std::string& device_width = "") {
    if (tech == "DDR3" || tech == "DDR4" || tech == "DDR5") {
        if (device_width == "x4")  return 16;
        if (device_width == "x16") return 4;
        return 8;                       // x8, the default 64-bit rank
    }
    return 1;
}

// Array read energy per 64B (act+col, 50% row-hit collapse). bank_override_pJ_per_byte
// > 0 forces the legacy bank-energy path (user knob); 0 = IDD default.
inline double arrayReadNJ(const std::string& tech, double tRC, double tRAS,
                          double tBurst, double bank_override_pJ_per_byte,
                          const std::string& device_width = "",
                          double row_miss_frac = -1.0) {
    if (bank_override_pJ_per_byte > 0.0)
        return bank_override_pJ_per_byte * 64.0 / 1000.0;
    IDDSpec s = iddFor(tech);
    double e_actpre_pJ = s.vdd * (s.idd0 * tRC - s.idd3n * tRAS - s.idd2n * (tRC - tRAS));
    double e_rd_pJ     = s.vdd * (s.idd4r - s.idd3n) * tBurst;
    /* 1.11.52 (audit D003): the activate/precharge share is MEASURED, not
     * assumed. It is the dominant term -- on DDR4 the act+pre part is ~1.42
     * nJ against ~0.39 nJ of burst -- and it used to be weighted by a
     * hardcoded ROW_MISS_FRAC = 0.5, so the largest number in the array
     * energy was a coin flip. The memory interface now tracks one open row
     * per unit and exports rowHits/rowMisses; the caller passes the measured
     * miss fraction. A negative value means the run carried no row
     * measurement, and the caller is responsible for saying so -- the 0.5
     * below is then the stated fallback, not a silent default. */
    const double ROW_MISS_FRAC = (row_miss_frac >= 0.0 && row_miss_frac <= 1.0)
                                 ? row_miss_frac : 0.5;
    return (ROW_MISS_FRAC * e_actpre_pJ + e_rd_pJ) / 1000.0
           * devicesPerAccess(tech, device_width);   // 1.11.46 (L181)
}
inline double arrayWriteNJ(const std::string& tech, double tRC, double tRAS,
                           double tBurst, double bank_override_pJ_per_byte,
                           const std::string& device_width = "",
                           double row_miss_frac = -1.0) {
    /* 1.11.5 (audit): writes consult IDD4W, not read*1.2. Same shape as the
     * read term: activate/precharge share plus the write burst current. */
    if (bank_override_pJ_per_byte > 0.0)
        return bank_override_pJ_per_byte * 64.0 / 1000.0 * 1.2;
    IDDSpec s = iddFor(tech);
    double e_actpre_pJ = s.vdd * (s.idd0 * tRC - s.idd3n * tRAS - s.idd2n * (tRC - tRAS));
    double e_wr_pJ     = s.vdd * (s.idd4w - s.idd3n) * tBurst;
    const double ROW_MISS_FRAC = (row_miss_frac >= 0.0 && row_miss_frac <= 1.0)
                                 ? row_miss_frac : 0.5;   // 1.11.52 (D003)
    return (ROW_MISS_FRAC * e_actpre_pJ + e_wr_pJ) / 1000.0
           * devicesPerAccess(tech, device_width);   // 1.11.46 (L181)
}

/* 1.11.5 (audit): interfaceNJ REMOVED. It returned vdd*(idd4r-idd3n)*tBurst
 * -- bit-identical to the burst term already inside arrayReadNJ, so every
 * consumer that added it double-charged the DQ read current. JEDEC IDD4R is
 * measured with the outputs driving: the on-die I/O switching is already in
 * the array term. The genuinely ADDITIONAL off-chip energy is termination
 * (below), which was computed and never charged. */

/* ODT/termination per 64B, per I/O standard. term_override_pJ_per_bit >= 0 =
 * user knob; termination_enable=false forces 0.
 *
 * 1.11.22 (user decision D12) -- DERIVED FROM THE JEDEC I/O STANDARDS the
 * technologies actually cite, replacing a per-scheme fudge factor whose
 * values the 1.11.15 audit showed were transposed. Two independent errors
 * were confirmed, both from normative text:
 *
 *  POD (DDR4 = POD12/JESD8-24, DDR5 = POD11, GDDR6 = POD135/JESD8-21C):
 *    "The POD driver uses a 40/60 Ohm output impedance that drives into a
 *     60 Ohm equivalent terminator tied to VDDQ" and "the terminator is
 *     disabled when the output driver is enabled" (JESD8-21C.01 cl.3);
 *    "signals ... are not generally expected to pull to VSS ... pull-up-only
 *     parallel input termination" (JESD8-25 cl.1).
 *    => driving HIGH the line sits at VDDQ and NO DC current flows; driving
 *       LOW the loop is the driver pull-down IN SERIES with the terminator.
 *       So duty ~0.5 for random data (we had 1.0), and the loop resistance
 *       is Rpd+Rtt (we used Rtt alone -- a further 100/60 = 1.67x). Combined
 *       we overstated GDDR6 termination by 3.33x.
 *
 *  SSTL (DDR3 = SSTL-15): terminated to VTT = VDDQ/2, so current flows in
 *    BOTH states (duty 1.0 -- we had 0.5) but the voltage ACROSS the
 *    terminated loop is VDDQ/2, not VDDQ. The two corrections partly cancel,
 *    which is why the transposition was not obvious in the totals.
 *    Every DDR3 constant below is normative, from JESD79-3D:
 *      Rpd = 34 Ohm: Table 38 "Output Driver DC Electrical Characteristics",
 *        RON34Pd/RON34Pu = RZQ/7 with RZQ = 240 Ohm; selected by MR1{A5,A1}
 *        = {0,1} (Figure 10). The other legal strength is RZQ/6 = 40 Ohm.
 *      Rtt = 40 Ohm: Table 41 "ODT DC Electrical Characteristics", RTT40 =
 *        RZQ/6, selected by MR1{A9,A6,A2} = {0,1,1} (Figure 10).
 *      Mid-rail: the SAME Table 41 row builds RTT40 from RTT40Pu80 and
 *        RTT40Pd80, each RZQ/3 = 80 Ohm, i.e. a split pull-up/pull-down pair
 *        whose Thevenin point is specified as "Deviation of VM w.r.t.
 *        VDDQ/2, DVM: -5/+5 %". That row IS the v_term = VDDQ/2 below; it is
 *        not an inference from the SSTL name.
 *
 *  LVSTL (LPDDR5): GROUND-REFERENCED, and it DOES terminate. This row
 *    returned 0 until 1.11.26 on the claim "unterminated by design". Micron's
 *    LPDDR5 datasheets say otherwise, in the feature list itself:
 *    "Programmable VSS on-die termination (ODT)", "Interface-LVSTL 0.5/0.3",
 *    "VDDQ = 0.50V or 0.45V TYP; 0.30V TYP (ODT off)", RON = 40 ohm
 *    (misc/MICT-S-A0025741931-1.pdf; misc/315b-441b-561b-y52q-*.pdf).
 *    So it is a THIRD topology: POD terminates to VDDQ, SSTL to a VDDQ/2
 *    mid-rail, LVSTL to VSS. Current flows while the driver holds the line
 *    HIGH -- the mirror of POD -- duty ~0.5 for unbiased data, loop = driver
 *    pull-up + terminator. The 0.5 V rail is what makes it cheap: against
 *    DDR5's 1.1 V that is 4.8x less V^2 before resistance divides.
 *    RESIDUAL, stated: Rtt = 240 ohm (RZQ, the LPDDR4/5 ODT reference) is the
 *    one UNSOURCED input. The part datasheet defers its ohm table to Micron's
 *    separate "General LPDDR5 Specifications 2: AC/DC and Interface", which we
 *    do not have. D12's IDD4R/IDD4W calibration can pin it -- the IDD tables
 *    ARE in hand (misc/MICT-S-A0025741931-1.pdf).
 *  HBM: 0, and now with a normative citation rather than physics reasoning:
 *    JESD238B.01 cl.9.1 measures HBM3 read-burst current with "IOUT = 0mA;
 *    Ctotal = 2.5 pF" -- an unterminated capacitive load.
 *
 * BOUNDARY (stated, not hidden): the 0.5 POD duty assumes an unbiased bit
 * stream. DBIac is enabled during JEDEC IDD measurement and deliberately
 * skews the LOW fraction, so the true duty is data-dependent; D12's IDD
 * cross-check is what pins that residual. */
inline double terminationNJ(const std::string& tech, double term_override_pJ_per_bit,
                            bool termination_enable = true) {
    if (!termination_enable) return 0.0;
    if (term_override_pJ_per_bit >= 0.0)
        return term_override_pJ_per_bit * 512.0 / 1000.0;

    /* 1.11.26: LVSTL is a THIRD topology, not an absence of one. Micron's
     * LPDDR5 datasheets state "Programmable VSS on-die termination (ODT)" with
     * VDDQ = 0.50 V nominal ODT-on and 0.30 V ODT-off, RON = 40 ohm
     * (misc/MICT-S-A0025741931-1.pdf, misc/315b-441b-561b-y52q-*.pdf). It
     * terminates to GROUND -- the mirror image of POD, which terminates to
     * VDDQ. So current flows while the driver holds the line HIGH, duty ~0.5
     * for random data, across a loop of driver pull-up plus terminator. */
    enum Scheme { POD, SSTL, LVSTL, NONE };
    Scheme sch; double vddq, rtt, rpd, mtps;
    /* 1.11.46 (FIX-PRE-FLEET L164): ONE PART per technology. The IDD row
     * above is sourced from Micron 4Gb DDR3L-1600 -- a 1.35 V part -- while
     * this line priced a 1.5 V SSTL-15 DDR3. Array and termination now
     * describe the SAME silicon: DDR3L, SSTL-135 (JESD79-3-1, the DDR3L
     * addendum keeps RZQ=240 and the T38/T41 RTT/RON tables at 1.35 V). */
    if      (tech=="DDR3")   {sch=SSTL; vddq=1.35; rtt=40;  rpd=34;  mtps=1600;}   // SSTL-135; JESD79-3-1 + T38/T41
                                                                                   // (RTT40=RZQ/6) + T38
                                                                                   // (RON34=RZQ/7), RZQ=240
    /* 1.11.52 (audit D002): mtps is the SIMULATED part's rate (DDR4-2400:
     * Ramulator preset DDR4_2400R and the architecture object), not a
     * different bin. POD12 (JESD8-24) is the interface standard and applies
     * at either rate, so vddq/rtt/rpd are untouched. */
    else if (tech=="DDR4")   {sch=POD;  vddq=1.2;  rtt=48;  rpd=40;  mtps=2400;}   // POD12  (JESD8-24)
    else if (tech=="DDR5")   {sch=POD;  vddq=1.1;  rtt=48;  rpd=40;  mtps=4800;}   // POD11  (same family)
    else if (tech=="GDDR6")  {sch=POD;  vddq=1.35; rtt=60;  rpd=40;  mtps=14000;}  // POD135 (JESD8-21C, Cl.D:
                                                                                   // RTT programmable 48/60 via MR6)
    /* 1.11.26: was NONE ("LVSTL, unterminated") -- wrong. LPDDR5 does
     * terminate; it terminates to VSS. VDDQ 0.5 V is the ODT-ON rail
     * (0.30 V is the ODT-off rail), RON 40 ohm from the same datasheets.
     * RTT: the datasheet defers the ohm table to Micron's separate
     * "General LPDDR5 Specifications 2: AC/DC and Interface" document, which
     * we do not have -- so 240 ohm (RZQ, the LPDDR4/5 ODT reference) is the
     * one UNSOURCED input here and is flagged as such below. */
    /* 1.11.52 (audit D008): the LPDDR5 Rtt below is the one UNSOURCED
     * electrical input in this table -- Micron's datasheets state
     * "programmable VSS ODT" and give VDDQ and RON but not the termination
     * value we need, so 240 ohm is an assumption, and it is 240 of the
     * 280-ohm loop (a 2x error in it moves LPDDR5 termination energy
     * ~1.75x). It was disclosed only in a comment 45 lines away; the
     * consumer now reports it at the point of use (see terminationNJ). */
    else if (tech=="LPDDR5") {sch=LVSTL; vddq=0.5; rtt=240; rpd=40; mtps=6400;}
    else if (tech.substr(0,3)=="HBM") return 0.0;                                  // interposer (JESD238B cl.9.1)
    else                     {sch=POD;  vddq=1.2;  rtt=48;  rpd=40;  mtps=3200;}
    if (sch == NONE) return 0.0;

    const double t_bit_s = 1.0 / (mtps * 1e6);
    double e_per_bit_pJ;
    if (sch == LVSTL) {
        /* Ground-referenced: the loop conducts while the line is HIGH, so the
         * duty is the complement of POD's but numerically the same 0.5 for
         * unbiased data. Loop = driver pull-up + terminator to VSS.
         * The low rail is what makes this cheap: 0.5 V against DDR5's 1.1 V
         * is a 4.8x reduction in V^2 before the resistance divides. */
        const double kHighDuty = 0.5;
        e_per_bit_pJ = kHighDuty * (vddq * vddq) / (rpd + rtt) * t_bit_s * 1e12;
    }
    else if (sch == POD) {
        // current only while LOW; loop = driver pull-down + terminator
        const double kLowDuty = 0.5;
        e_per_bit_pJ = kLowDuty * (vddq * vddq) / (rpd + rtt) * t_bit_s * 1e12;
    } else {
        // SSTL: VTT = VDDQ/2 across the loop, drawn in both states
        const double v_term = vddq * 0.5;
        e_per_bit_pJ = (v_term * v_term) / (rpd + rtt) * t_bit_s * 1e12;
    }
    return e_per_bit_pJ * 512.0 / 1000.0;
}

/* All three quantities below are PER IDD-BEARING UNIT: one DDR-class chip,
 * or one HBM channel. That is the unit the JEDEC IDD tables are written
 * against. Multiplying up to the memory system is backgroundUnits() and is
 * done once, in backgroundSystemMW(). (Before 1.11.20 there was no
 * multiplication at all: an HBM stack's 8-16 channels and a DDR rank's 8
 * chips were each reported as a single device's background.) */
/* 1.11.37 (audit E15): refresh charged PER STATE.
 *
 * JEDEC IDD5 is the all-bank auto-refresh current, measured with the banks
 * precharged; the device draws it for tRFC out of every tREFI. It is an
 * ABSOLUTE current, not an increment, and a device must EXIT power-down to
 * accept a REFRESH command -- so for the tRFC/tREFI duty fraction the unit
 * draws IDD5 whatever state it was otherwise holding, and its own state
 * current for the remainder.
 *
 *     P(state) = vdd * ( idd_state * (1 - duty) + idd5 * duty )
 *
 * That expression is state-independent, which the previous structure was not:
 * refreshMW() below returns the excess over IDD3N, correct only when added to
 * an IDD3N baseline, and backgroundUnitMW() added it over the IDLE fraction
 * too, where the baseline is IDD2N or IDD2P. Since IDD2P < IDD3N, refresh was
 * UNDER-charged during power-down by vdd*(idd3n-idd2p)*duty -- for HBM3,
 * 1.1 * (22-7) * (160/3900) = 0.68 mW/unit, ~2.6% of the 26.4 mW per-unit
 * background, 10.8 mW across a 16-channel stack at full idle. Inert on the
 * present corpus (1.11.20 measured r_idle = 0: memory-bound kernels keep the
 * controller busy every phase), so this is a correctness fix, not a results
 * change -- backgroundUnitMW is bit-identical at r_idle = 0 by construction.
 *
 * The idd5 clamp guards a table row where IDD5 < the state current, which
 * would otherwise let refresh REDUCE a unit's power. */
inline double stateWithRefreshMW(const IDDSpec& s, double idd_state) {
    const double duty = (s.trefi_ns > 0.0) ? (s.trfc_ns / s.trefi_ns) : 0.0;
    const double idd5 = (s.idd5 > idd_state) ? s.idd5 : idd_state;
    return s.vdd * (idd_state * (1.0 - duty) + idd5 * duty);
}

/* The refresh EXCESS over active standby. Reported as its own line item, and
 * that is the only thing it means: it is IDD3N-relative and must not be added
 * to a baseline that is not IDD3N. backgroundUnitMW() no longer calls it. */
inline double refreshMW(const std::string& tech) {
    IDDSpec s = iddFor(tech);
    return s.vdd * (s.idd5 - s.idd3n) * (s.trfc_ns / s.trefi_ns);
}
inline double backgroundMW(const std::string& tech) {
    IDDSpec s = iddFor(tech);
    return stateWithRefreshMW(s, s.idd3n);   // == vdd*idd3n + refreshMW(tech)
}

/* 1.11.20 (user decision D13): POPULATION. How many IDD-bearing units the
 * memory system presents behind one channel.
 *
 * Deliberately derived from technology + JEDEC device width and NOT from
 * config.hierarchy_chips_per_rank, even though that field holds the same
 * numbers: the hierarchy field is degenerated to 1 for HOST_MC placement
 * (main.cpp, the PEs-share-the-host-MC path), because it is an address-
 * mapping fanout there. The number of chips physically drawing standby
 * current does not depend on where the PEs sit, so reading that field would
 * have silently zeroed the correction for exactly the baseline placement.
 *
 *   HBM2/HBM3   channels per stack (8 / 16), from IDDSpec::channels, which
 *               was declared in 1.11.8 for this purpose and never read.
 *               JESD238B.01 cl.9.1 specifies HBM IDD per channel.
 *   DDR3/4/5    chips per rank for a 64-bit channel: x4 -> 16, x8 -> 8,
 *               x16 -> 4. Mirrors the device-width table in main.cpp.
 *   LPDDR5      1: an x16 die serves its own channel.
 *   GDDR6       1: point-to-point, one device per channel.
 *   SRAM/NVM    1 (they do not reach this path; the fallthrough is DDR4). */
/* 1.11.52 (audit A015): the POPULATION arguments. This returns the devices
 * in ONE rank (DDR) or ONE stack (HBM); the SYSTEM may hold several ranks
 * and channels, and the area path already counts them all
 * (memorySystemDieCount = chips x ranks x channels). The two lines of one
 * report therefore described memories differing by ranks x channels: e.g.
 * two ranks put 54 mm^2 of memory area beside 1.12 W of single-rank memory
 * power. Callers pass the system's rank and channel counts so background
 * power is population-scaled the same way area is; the defaults reproduce
 * the pre-1.11.52 single-rank basis for any caller not yet updated. */
inline int backgroundUnits(const std::string& tech,
                           const std::string& device_width = "",
                           int ranks_per_channel = 1,
                           int channels = 1) {
    if (ranks_per_channel < 1) ranks_per_channel = 1;
    if (channels < 1) channels = 1;
    if (tech.substr(0, 3) == "HBM") {
        /* An HBM stack's channels ARE its population; a system with several
         * stacks multiplies by the channel count the caller reports. */
        int ch = iddFor(tech).channels;
        int per_stack = ch > 0 ? ch : 1;
        int stacks = (channels > per_stack && per_stack > 0)
                     ? (channels / per_stack) : 1;
        return per_stack * stacks;
    }
    if (tech == "DDR3" || tech == "DDR4" || tech == "DDR5") {
        int chips = 8;                  // x8, the default 64-bit rank
        if (device_width == "x4")  chips = 16;
        if (device_width == "x16") chips = 4;
        return chips * ranks_per_channel * channels;
    }
    return ranks_per_channel * channels;
}

/* 1.11.20 (user decision D15): STATE. Background power for ONE unit, given
 * the measured no-traffic residency r_idle. Three JEDEC states, not two:
 *
 *   busy  (1 - r_idle)  ACTIVE STANDBY, IDD3N -- a row is open.
 *   idle, pg off        PRECHARGE STANDBY, IDD2N. An idle controller closes
 *                       its pages; that descent is PAGE POLICY and happens
 *                       whether or not a power-management feature exists.
 *                       1.11.18 identified this correctly but left the
 *                       baseline at IDD3N to preserve pg-off bit-identity,
 *                       which meant the baseline stayed knowingly wrong.
 *                       D15 fixes the baseline instead.
 *   idle, pg on         PRECHARGE POWER-DOWN, IDD2P (CKE low).
 *
 * Refresh continues in ALL states -- DRAM must retain -- so every state pays
 * it, each against its OWN baseline (1.11.37, E15). Before that it was added
 * as one IDD3N-relative term on top of all three states, which under-charged
 * refresh during power-down.
 *
 * tXP hysteresis: entry/exit overhead means a small slice of the idle time
 * cannot reach power-down. Phases are 10k cycles and tXP is ~10 ns, so that
 * slice is <1%; the 0.99 factor states it rather than ignoring it. The slice
 * that fails to reach IDD2P sits at IDD2N, not at IDD3N -- it is still idle.
 *
 * DELIBERATE BASELINE CHANGE: r_idle > 0 with pg OFF no longer reproduces
 * backgroundMW(). That was the point of D15, and it is why the 1.11.20 gate
 * asserts a stated delta for DRAM cells rather than bit-equality. */
inline double backgroundUnitMW(const std::string& tech, double r_idle,
                               bool pg_enabled) {
    if (r_idle < 0.0) r_idle = 0.0;
    if (r_idle > 1.0) r_idle = 1.0;
    IDDSpec s = iddFor(tech);
    const double kHysteresisDerate = 0.99;
    /* Each state pays its OWN refresh (E15) -- see stateWithRefreshMW. */
    const double active_mw = stateWithRefreshMW(s, s.idd3n);  // IDD3N, row open
    const double pre_mw    = stateWithRefreshMW(s, s.idd2n);  // IDD2N, precharged
    double pd_mw           = stateWithRefreshMW(s, s.idd2p);  // IDD2P, CKE low
    if (pd_mw > pre_mw) pd_mw = pre_mw;         // guard odd rows: never a penalty
    double idle_mw;
    if (pg_enabled) {
        const double r_pd = r_idle * kHysteresisDerate;
        idle_mw = pd_mw * r_pd + pre_mw * (r_idle - r_pd);
    } else {
        idle_mw = pre_mw * r_idle;
    }
    return active_mw * (1.0 - r_idle) + idle_mw;
}

/* The memory system's background: population x per-unit state-aware power.
 * This is the only function the report should call. */
inline double backgroundSystemMW(const std::string& tech, double r_idle,
                                 bool pg_enabled,
                                 const std::string& device_width = "",
                                 int ranks_per_channel = 1,
                                 int channels = 1) {   // 1.11.52 (A015)
    return backgroundUnitMW(tech, r_idle, pg_enabled) *
           static_cast<double>(backgroundUnits(tech, device_width,
                                               ranks_per_channel, channels));
}

} // namespace pimid_energy
} // namespace Ramulator
#endif
