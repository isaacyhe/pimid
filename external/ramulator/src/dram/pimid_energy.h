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
// The per-tech IDD/VDD tables here are the datasheet-class values also declared as
// current_presets/voltage_presets in the per-impl device classes (DDR3/4/5, LPDDR5,
// GDDR6, HBM2/3), following upstream convention. Ramulator2's own command-driven
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

// Array read energy per 64B (act+col, 50% row-hit collapse). bank_override_pJ_per_byte
// > 0 forces the legacy bank-energy path (user knob); 0 = IDD default.
inline double arrayReadNJ(const std::string& tech, double tRC, double tRAS,
                          double tBurst, double bank_override_pJ_per_byte) {
    if (bank_override_pJ_per_byte > 0.0)
        return bank_override_pJ_per_byte * 64.0 / 1000.0;
    IDDSpec s = iddFor(tech);
    double e_actpre_pJ = s.vdd * (s.idd0 * tRC - s.idd3n * tRAS - s.idd2n * (tRC - tRAS));
    double e_rd_pJ     = s.vdd * (s.idd4r - s.idd3n) * tBurst;
    const double ROW_MISS_FRAC = 0.5;
    return (ROW_MISS_FRAC * e_actpre_pJ + e_rd_pJ) / 1000.0;
}
inline double arrayWriteNJ(const std::string& tech, double tRC, double tRAS,
                           double tBurst, double bank_override_pJ_per_byte) {
    /* 1.11.5 (audit): writes consult IDD4W, not read*1.2. Same shape as the
     * read term: activate/precharge share plus the write burst current. */
    if (bank_override_pJ_per_byte > 0.0)
        return bank_override_pJ_per_byte * 64.0 / 1000.0 * 1.2;
    IDDSpec s = iddFor(tech);
    double e_actpre_pJ = s.vdd * (s.idd0 * tRC - s.idd3n * tRAS - s.idd2n * (tRC - tRAS));
    double e_wr_pJ     = s.vdd * (s.idd4w - s.idd3n) * tBurst;
    const double ROW_MISS_FRAC = 0.5;
    return (ROW_MISS_FRAC * e_actpre_pJ + e_wr_pJ) / 1000.0;
}

/* 1.11.5 (audit): interfaceNJ REMOVED. It returned vdd*(idd4r-idd3n)*tBurst
 * -- bit-identical to the burst term already inside arrayReadNJ, so every
 * consumer that added it double-charged the DQ read current. JEDEC IDD4R is
 * measured with the outputs driving: the on-die I/O switching is already in
 * the array term. The genuinely ADDITIONAL off-chip energy is termination
 * (below), which was computed and never charged. */

// ODT/termination per 64B, per I/O standard. term_override_pJ_per_bit >= 0 = user knob.
// termination_enable=false forces 0. HBM = 0 by physics (interposer microbumps).
inline double terminationNJ(const std::string& tech, double term_override_pJ_per_bit,
                            bool termination_enable = true) {
    if (!termination_enable) return 0.0;
    if (term_override_pJ_per_bit >= 0.0)
        return term_override_pJ_per_bit * 512.0 / 1000.0;
    double vddq, rtt, mtps, scheme = 1.0;
    if      (tech=="DDR3")   {vddq=1.5;  rtt=40;  mtps=1600;  scheme=0.5;}  // SSTL-15 -> VTT mid-rail
    else if (tech=="DDR4")   {vddq=1.2;  rtt=48;  mtps=3200;  scheme=1.0;}  // POD12
    else if (tech=="DDR5")   {vddq=1.1;  rtt=48;  mtps=4800;  scheme=1.0;}  // POD11
    else if (tech=="GDDR6")  {vddq=1.35; rtt=50;  mtps=14000; scheme=1.0;}  // POD135
    else if (tech=="LPDDR5") {vddq=0.5;  rtt=240; mtps=6400;  scheme=0.0;}  // LVSTL, unterminated
    else if (tech.substr(0,3)=="HBM") return 0.0;                          // interposer
    else {vddq=1.2; rtt=48; mtps=3200; scheme=1.0;}
    double t_bit_s = 1.0 / (mtps * 1e6);
    double e_per_bit_pJ = scheme * ((vddq * vddq) / rtt) * t_bit_s * 1e12;
    return e_per_bit_pJ * 512.0 / 1000.0;
}

inline double refreshMW(const std::string& tech) {
    IDDSpec s = iddFor(tech);
    return s.vdd * (s.idd5 - s.idd3n) * (s.trfc_ns / s.trefi_ns);
}
inline double backgroundMW(const std::string& tech) {
    IDDSpec s = iddFor(tech);
    return s.vdd * s.idd3n + refreshMW(tech);
}

/* 1.11.8 (#84): background power under power-down descent. During measured
 * no-traffic residency r_idle the device sits in precharge power-down
 * (IDD2P, CKE low) instead of active standby; refresh continues in ALL
 * states (DRAM must retain). A tXP-scale hysteresis discount derates the
 * idle fraction so few-cycle gaps are not credited: phases are 10k cycles,
 * tXP is ~10ns, so entry/exit overhead within a genuinely idle phase is
 * <1% -- the derate factor 0.99 states it rather than ignoring it.
 * r_idle=0 reproduces backgroundMW exactly (PG-off invariant). */
inline double backgroundEffectiveMW(const std::string& tech, double r_idle) {
    if (r_idle <= 0.0) return backgroundMW(tech);
    if (r_idle > 1.0) r_idle = 1.0;
    IDDSpec s = iddFor(tech);
    const double kHysteresisDerate = 0.99;
    double r = r_idle * kHysteresisDerate;
    double standby_mw  = s.vdd * s.idd3n;
    double pd_mw       = s.vdd * s.idd2p;
    return standby_mw * (1.0 - r) + pd_mw * r + refreshMW(tech);
}

} // namespace pimid_energy
} // namespace Ramulator
#endif
