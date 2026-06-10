/*
 * QEMU Plugin API - Vendored header for PIMID/ZSim QEMU integration.
 *
 * This is a minimal reproduction of the QEMU 9.2 TCG plugin API (v2),
 * containing only the types and function declarations needed by
 * PIMID's trace and simulation plugins.
 *
 * Original source: QEMU project, include/qemu/qemu-plugin.h
 * License: GNU General Public License version 2 or later
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QEMU_PLUGIN_H
#define QEMU_PLUGIN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * For best performance, build with -fvisibility=hidden so that
 * QEMU_PLUGIN_EXPORT is meaningful.
 */
#if defined(__GNUC__) || defined(__clang__)
#define QEMU_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
#define QEMU_PLUGIN_EXPORT
#endif

/* Opaque types provided by QEMU to plugins */
typedef uint64_t qemu_plugin_id_t;

struct qemu_plugin_tb;
struct qemu_plugin_insn;
typedef uint32_t qemu_plugin_meminfo_t;

/* Plugin version — must match QEMU's expected version.
 * QEMU 8.x uses version 1, QEMU 9.x minimum version is 2. */
#define QEMU_PLUGIN_VERSION 2

/*
 * qemu_info_t - system information provided to plugin at install time
 */
typedef struct qemu_info_t {
    const char *target_name;
    struct {
        int min;
        int cur;
    } version;
    bool system_emulation;
    union {
        struct {
            uint64_t entry;
        } user;
        struct {
            int smp_vcpus;
            int max_vcpus;
        } system;
    };
} qemu_info_t;

/* ---- Callback types ---- */

typedef void (*qemu_plugin_simple_cb_t)(qemu_plugin_id_t id);

typedef void (*qemu_plugin_vcpu_simple_cb_t)(qemu_plugin_id_t id,
                                              unsigned int vcpu_index);

typedef void (*qemu_plugin_vcpu_tb_trans_cb_t)(qemu_plugin_id_t id,
                                                struct qemu_plugin_tb *tb);

typedef void (*qemu_plugin_vcpu_udata_cb_t)(unsigned int vcpu_index,
                                             void *userdata);

typedef void (*qemu_plugin_vcpu_mem_cb_t)(unsigned int vcpu_index,
                                           qemu_plugin_meminfo_t info,
                                           uint64_t vaddr,
                                           void *userdata);

typedef void (*qemu_plugin_vcpu_syscall_cb_t)(qemu_plugin_id_t id,
                                               unsigned int vcpu_index,
                                               int64_t num,
                                               uint64_t a1, uint64_t a2,
                                               uint64_t a3, uint64_t a4,
                                               uint64_t a5, uint64_t a6,
                                               uint64_t a7, uint64_t a8);

typedef void (*qemu_plugin_vcpu_syscall_ret_cb_t)(qemu_plugin_id_t id,
                                                    unsigned int vcpu_index,
                                                    int64_t num,
                                                    int64_t ret);

/* ---- Enum types ---- */

enum qemu_plugin_cb_flags {
    QEMU_PLUGIN_CB_NO_REGS  = 0,
    QEMU_PLUGIN_CB_R_REGS   = 1,
    QEMU_PLUGIN_CB_RW_REGS  = 2,
};

enum qemu_plugin_mem_rw {
    QEMU_PLUGIN_MEM_R  = 1,
    QEMU_PLUGIN_MEM_W  = 2,
    QEMU_PLUGIN_MEM_RW = 3,
};

enum qemu_plugin_op {
    QEMU_PLUGIN_INLINE_ADD_U64,
};

/* ---- Plugin lifecycle ---- */

QEMU_PLUGIN_EXPORT void qemu_plugin_uninstall(qemu_plugin_id_t id,
                                                qemu_plugin_simple_cb_t cb);

QEMU_PLUGIN_EXPORT void qemu_plugin_reset(qemu_plugin_id_t id,
                                            qemu_plugin_simple_cb_t cb);

typedef void (*qemu_plugin_udata_cb_t)(qemu_plugin_id_t id, void *userdata);

QEMU_PLUGIN_EXPORT void qemu_plugin_register_atexit_cb(
    qemu_plugin_id_t id, qemu_plugin_udata_cb_t cb, void *userdata);

/* ---- vCPU lifecycle ---- */

QEMU_PLUGIN_EXPORT void qemu_plugin_register_vcpu_init_cb(
    qemu_plugin_id_t id, qemu_plugin_vcpu_simple_cb_t cb);

QEMU_PLUGIN_EXPORT void qemu_plugin_register_vcpu_exit_cb(
    qemu_plugin_id_t id, qemu_plugin_vcpu_simple_cb_t cb);

/* ---- Translation block instrumentation ---- */

QEMU_PLUGIN_EXPORT void qemu_plugin_register_vcpu_tb_trans_cb(
    qemu_plugin_id_t id, qemu_plugin_vcpu_tb_trans_cb_t cb);

/* ---- TB / instruction queries ---- */

QEMU_PLUGIN_EXPORT size_t qemu_plugin_tb_n_insns(
    const struct qemu_plugin_tb *tb);

QEMU_PLUGIN_EXPORT uint64_t qemu_plugin_tb_vaddr(
    const struct qemu_plugin_tb *tb);

QEMU_PLUGIN_EXPORT struct qemu_plugin_insn *qemu_plugin_tb_get_insn(
    const struct qemu_plugin_tb *tb, size_t idx);

QEMU_PLUGIN_EXPORT uint64_t qemu_plugin_insn_vaddr(
    const struct qemu_plugin_insn *insn);

QEMU_PLUGIN_EXPORT size_t qemu_plugin_insn_size(
    const struct qemu_plugin_insn *insn);

QEMU_PLUGIN_EXPORT size_t qemu_plugin_insn_data(
    const struct qemu_plugin_insn *insn,
    void *dest, size_t len);

QEMU_PLUGIN_EXPORT char *qemu_plugin_insn_disas(
    const struct qemu_plugin_insn *insn);

QEMU_PLUGIN_EXPORT const char *qemu_plugin_insn_symbol(
    const struct qemu_plugin_insn *insn);

/* ---- Per-instruction callbacks ---- */

QEMU_PLUGIN_EXPORT void qemu_plugin_register_vcpu_insn_exec_cb(
    struct qemu_plugin_insn *insn,
    qemu_plugin_vcpu_udata_cb_t cb,
    enum qemu_plugin_cb_flags flags,
    void *userdata);

/* ---- Memory access callbacks ---- */

QEMU_PLUGIN_EXPORT void qemu_plugin_register_vcpu_mem_cb(
    struct qemu_plugin_insn *insn,
    qemu_plugin_vcpu_mem_cb_t cb,
    enum qemu_plugin_cb_flags flags,
    enum qemu_plugin_mem_rw rw,
    void *userdata);

/* Memory info helpers */
QEMU_PLUGIN_EXPORT bool qemu_plugin_mem_is_store(qemu_plugin_meminfo_t info);
QEMU_PLUGIN_EXPORT unsigned int qemu_plugin_mem_size_shift(
    qemu_plugin_meminfo_t info);

static inline unsigned int qemu_plugin_mem_size(qemu_plugin_meminfo_t info) {
    return 1u << qemu_plugin_mem_size_shift(info);
}

/* ---- Inline operations (lightweight, no callback overhead) ---- */

QEMU_PLUGIN_EXPORT void qemu_plugin_register_vcpu_tb_exec_inline(
    struct qemu_plugin_tb *tb,
    enum qemu_plugin_op op,
    void *ptr, uint64_t imm);

QEMU_PLUGIN_EXPORT void qemu_plugin_register_vcpu_insn_exec_inline(
    struct qemu_plugin_insn *insn,
    enum qemu_plugin_op op,
    void *ptr, uint64_t imm);

/* ---- Syscall callbacks ---- */

QEMU_PLUGIN_EXPORT void qemu_plugin_register_vcpu_syscall_cb(
    qemu_plugin_id_t id, qemu_plugin_vcpu_syscall_cb_t cb);

QEMU_PLUGIN_EXPORT void qemu_plugin_register_vcpu_syscall_ret_cb(
    qemu_plugin_id_t id, qemu_plugin_vcpu_syscall_ret_cb_t cb);

/* ---- Output ---- */

QEMU_PLUGIN_EXPORT void qemu_plugin_outs(const char *string);

/* ---- User-mode helpers ---- */

QEMU_PLUGIN_EXPORT const char *qemu_plugin_path_to_binary(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* QEMU_PLUGIN_H */
