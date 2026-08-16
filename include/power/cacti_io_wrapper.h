/* PIMID 1.11.40 -- harness for CACTI-IO (external/cacti/extio.cc).
 *
 * WHY THIS EXISTS. The link surface was a table of DERIVED numbers: 7.0 pJ/bit,
 * 63.0 GB/s, 500 ns per transaction, and "UCIe PHY area is not sourceable".
 * Each was a single value standing where a model belongs. CACTI-IO is an
 * off-chip IO model that computes termination power, PHY static and dynamic
 * power, IO area and timing/voltage margin from EXTRACTED PARAMETERS -- swing
 * voltages, capacitances, bias and leakage currents, termination resistances,
 * and area coefficients as a polynomial in impedance and frequency. It has been
 * vendored in this tree, compiled into the CACTI library, and called by nothing.
 *
 * WHAT IT COVERS -- MEASURED, not assumed (gate 1155, audit N8). CACTI-IO's
 * Mem_IO_type spans DDR3, DDR4, LPDDR2, WideIO, Low_Swing_Diff and Serial. We
 * ran all of them:
 *
 *   DRAM-CLASS PATHS: USABLE. DDR4-3200 x64 gives IO area 2.99 mm^2,
 *   termination 262 mW, dynamic 443 mW, PHY 390 mW -- about 1.1 W, physical.
 *   This is the model's home and the reason to harness it: it replaces the
 *   hand-written SSTL/POD/LVSTL scheme table in pimid_energy.h and adds PHY
 *   power and IO area, which PIMID does not model at all.
 *
 *   SERIAL PATH: NOT USABLE, and it had never been executed by anyone. Four
 *   independent faults, all upstream: (1) nothing called it; (2) the Serial
 *   branch declared r_diff_term and left rtt1/rtt2_dq_*, rs1/rs2_dq, rtt_ca,
 *   r_stub_ca, z0, r_on and t_flight at ZERO, so rpar_write divided by zero --
 *   SIGFPE on construction; (3) num_mem_clk divides by (num_clk/2) and a
 *   serial link legitimately has num_clk = 0 (embedded clock, CDR), so integer
 *   0/2 faults; (4) the area polynomial is a FIT over DDR frequencies -- its
 *   cubic term is 0.003 at 800 MHz and 163.8 at 32 GHz, yielding 61 mm^2 of IO
 *   area for 16 lanes, and even at 8 GT/s the termination power returns
 *   3.97e+280 mW (uninitialised memory).
 *   1.11.40 repairs (2) and (3) in our fork so the path constructs, and
 *   REFUSES above 8000 MHz rather than extrapolating the cubic. It does NOT
 *   repair (4): refitting coefficients for multi-GHz serial rates cannot be
 *   done from published bands alone.
 *
 * So PCIe/CXL link energy stays on the 1.11.40 published bands, labelled as
 * literature values rather than model output. DSENT is the candidate model for
 * that class and is not vendored.
 *
 * VALIDATION TARGET. The published bands (PCIe gen5 7.6-11.4 pJ/bit; UCIe
 * advanced 0.25-0.5) are the test set for whatever model eventually covers
 * serial links -- not the model itself.
 *
 * GLOBAL-STATE DISCIPLINE (audit E30). CACTI-IO reads g_ip -- num_dq, num_dqs,
 * num_ca, num_clk, duty_cycle, io_type, iostate -- the same global CACTIWrapper
 * and the McPAT fork also use. E30 records that CACTIWrapper leaves it dangling.
 * This wrapper therefore SAVES the whole InputParameter, mutates only the IO
 * fields it needs, and RESTORES it, so adding a second consumer cannot turn
 * into a race between callers.
 */
#ifndef PIMID_POWER_CACTI_IO_WRAPPER_H
#define PIMID_POWER_CACTI_IO_WRAPPER_H

#include <string>

namespace PIMID {

/* One evaluation of a link's IO. Powers in mW, area in mm^2. */
struct LinkIOResult {
    bool   valid = false;

    double io_area_mm2 = 0.0;          // driver + ODT active circuit, per link
    double io_power_term_mw = 0.0;     // termination (duty-cycle weighted)
    double io_power_dynamic_mw = 0.0;  // switching on dq/dqs/ca/clk
    double phy_power_mw = 0.0;         // static + dynamic PHY
    double phy_static_power_mw = 0.0;
    double phy_dynamic_power_mw = 0.0;

    double timing_margin_ui = 0.0;     // eye margins, reported not consumed
    double voltage_margin_v = 0.0;

    /* Derived for comparison against the published bands. This is the whole
     * point of the harness: a number the model PRODUCED, next to the numbers
     * the literature reports. */
    double energy_pj_per_bit = 0.0;
    /* Termination alone, for a like-for-like swap against the hand-written
     * scheme table in pimid_energy.h, which models ONLY termination. Keeping
     * the components separable is what lets the replacement be checked term by
     * term instead of as one aggregate that could hide a compensating error. */
    double energy_pj_per_bit_term = 0.0;

    /* Is the technology->Mem_IO_type map EXACT, or a nearest neighbour? Only
     * exact maps are allowed to replace an existing result. An approximate map
     * (LPDDR5 -> LPDDR2, HBM -> WideIO) is a cross-check to report, never a
     * substitution to make silently: its parameter set was fitted for a
     * different interface and its absolute values are not trustworthy. */
    bool exact_map = false;

    std::string source;                // which parameter set was used
    std::string not_modelled;          // what this result does NOT include
};

class CactiIOWrapper {
public:
    /* Evaluate a link.
     *   link_type   PIMID's link vocabulary (pcie_gen5, cxl_*, interposer, ...)
     *   num_lanes   signal count -- the thing that scales area and dynamic power
     *   rate_gt_s   per-lane transfer rate, a specification fact
     *   duty_cycle  MEASURED activity (1.11.40 E19), not a configured constant
     * Returns valid=false with `source` explaining why when the link type has
     * no CACTI-IO counterpart; the caller must not substitute a guess. */
    static LinkIOResult computeLink(const std::string& link_type,
                                    int num_lanes,
                                    double rate_gt_s,
                                    double duty_cycle);

    /* Evaluate a DRAM INTERFACE -- CACTI-IO's validated home range, and the
     * reason this harness is worth having. Replaces the hand-written
     * SSTL/POD/LVSTL scheme table in pimid_energy.h (typed-in vddq/rtt/rpd)
     * with extracted parameters, and additionally yields PHY power and IO
     * AREA, neither of which PIMID models today.
     *   tech        PIMID memory technology name
     *   num_dq      data pins on the channel (64 for a DDR-class channel)
     *   rate_mts    transfer rate MT/s -- a specification fact
     *   activity    MEASURED read/write activity fraction, not a constant
     * Returns valid=false with `source` naming the reason when the technology
     * has no CACTI-IO counterpart. The mapping is approximate for technologies
     * postdating the model and every approximation is named in `source`. */
    static LinkIOResult computeDramIO(const std::string& tech,
                                      int num_dq,
                                      double rate_mts,
                                      double activity);

    /* DRAM specification primitives -- rate and channel width. Both are fixed
     * by the standard, so they are the legitimate kind of table entry. <0 for
     * unknown technologies. */
    static double dramRateMTs(const std::string& tech);
    static int    dramChannelWidthBits(const std::string& tech);

    /* Link CONTROLLER clock from specification primitives (1.11.42, E21):
     *     clock_MHz = rate_GT/s * 1000 / pipe_width_bits
     * The PIPE datapath width is bounded by the PIPE spec (8/16/32-bit per
     * lane; Intel PIPE Architecture Spec rev 7.1) and 32-bit is the common
     * configuration for gen3+ -- an implementation CONVENTION, stated, not a
     * per-generation constant pretending to be one. Returns <0 for link
     * families that do not use PIPE (NVLink, UALink, UCIe) -- their controller
     * clocks have no sourced basis here and the caller must say so rather
     * than defaulting. */
    static double linkControllerClockMHz(const std::string& link_type);

    /* Bandwidth from specification primitives, replacing pcie_bandwidth_GBs.
     * lanes x rate x encoding efficiency / 8. Returns <0 for unknown types. */
    static double linkBandwidthGBs(const std::string& link_type, int num_lanes);

    /* Per-lane rate and encoding for a link type -- specification facts, the
     * only kind of single value that belongs in a table. <0 if unknown. */
    static double linkRateGTs(const std::string& link_type);
    static double linkEncodingEfficiency(const std::string& link_type);
};

}  // namespace PIMID
#endif
