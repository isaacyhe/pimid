/**
 * @file cacti7_compat.h
 * @brief Compatibility layer for McPAT to use CACTI 7.0 instead of bundled CACTI 6.5-P
 *
 * This header provides compatibility definitions that allow McPAT to work with
 * CACTI 7.0. The main differences between CACTI 6.5-P (McPAT's bundled version)
 * and CACTI 7.0 are:
 *
 * 1. powerComponents class:
 *    - CACTI 6.5-P has power_gated_leakage, power_gated_with_long_channel_leakage
 *    - CACTI 7.0 does NOT have these fields
 *
 * 2. InputParameter class:
 *    - CACTI 7.0 adds 3D DRAM, TSV, and memory I/O parameters
 *    - CACTI 7.0 removes some CACTI 6.5-P specific voltage/vdd parameters
 *
 * 3. uca_org_t class:
 *    - CACTI 6.5-P has uca_q and uca_pg_reference for power gating results
 *    - CACTI 7.0 does NOT have these
 *
 * 4. mem_array class:
 *    - CACTI 7.0 adds 3D DRAM stats, energy breakdowns
 *    - CACTI 6.5-P has long_channel_leakage_reduction fields
 *
 * VALIDATION NOTE:
 * The core CACTI algorithms for SRAM/cache modeling remain similar between
 * versions. For typical McPAT usage (cache hierarchy power estimation),
 * results should be comparable. However:
 *
 * - Power gating features from McPAT that relied on CACTI 6.5-P specific
 *   fields will return 0 (no power gating contribution)
 * - Technology parameters may differ slightly due to updated models in CACTI 7.0
 * - For validation, compare results on same configurations between versions
 *
 * Usage: This file is included AFTER the main CACTI 7.0 cacti_interface.h
 */

#ifndef MCPAT_CACTI7_COMPAT_H
#define MCPAT_CACTI7_COMPAT_H

// Only apply compatibility if using CACTI 7.0
#ifdef HAVE_CACTI7

// Compatibility: Add missing power gating fields to powerComponents
// These are used by McPAT but not present in CACTI 7.0
// We provide them as macros that return 0 when accessed

// Helper struct to provide power gating values (always 0 for CACTI 7.0)
// This is used when McPAT accesses these non-existent fields
namespace mcpat_cacti7_compat {

// For any powerComponents object, these fields don't exist in CACTI 7.0
// McPAT code that accesses them will use these default values

inline double get_power_gated_leakage() { return 0.0; }
inline double get_power_gated_with_long_channel_leakage() { return 0.0; }

// For InputParameter compatibility
inline bool get_specific_hp_vdd_default() { return false; }
inline double get_hp_Vdd_default() { return 1.0; }
inline bool get_specific_lstp_vdd_default() { return false; }
inline double get_lstp_Vdd_default() { return 1.0; }
inline bool get_specific_lop_vdd_default() { return false; }
inline double get_lop_Vdd_default() { return 1.0; }
inline bool get_specific_vcc_min_default() { return false; }
inline double get_user_defined_vcc_min_default() { return 0.0; }
inline bool get_user_defined_vcc_underflow_default() { return false; }
inline bool get_long_channel_device_default() { return false; }

// For uca_org_t compatibility
// uca_q and uca_pg_reference don't exist in CACTI 7.0
// McPAT should not use them when CACTI 7.0 is active

// For mem_array compatibility
inline double get_long_channel_leakage_reduction_periperal() { return 1.0; }
inline double get_long_channel_leakage_reduction_memcell() { return 1.0; }

} // namespace mcpat_cacti7_compat

// Macro to safely access power_gated_leakage on powerComponents
// In CACTI 7.0, this field doesn't exist, so return 0
#define MCPAT_POWER_GATED_LEAKAGE(pc) (0.0)
#define MCPAT_POWER_GATED_WITH_LONG_CHANNEL_LEAKAGE(pc) (0.0)

// Note: The above approach requires McPAT code modifications to use these macros
// An alternative approach (used below) is to not modify McPAT code at all,
// but ensure the fields exist at compile time by detecting version.

#endif // HAVE_CACTI7

#endif // MCPAT_CACTI7_COMPAT_H
