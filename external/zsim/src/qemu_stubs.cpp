/**
 * Stubs for functions not available in the QEMU plugin build.
 *
 * These functions are referenced by ZSim core files (scheduler.cpp,
 * init.cpp, trace_driver.cpp) but rely on infrastructure not present
 * in the QEMU user-mode context (Pin virtual-machine introspection).
 * Providing no-op stubs allows the plugin to link without pulling in
 * those subsystems.
 */

#include <cstdint>
#include "galloc.h"

/* ---- virt/syscall_name.h ---- */
/* Declared as: const char* GetSyscallName(uint32_t syscall); */
const char* GetSyscallName(uint32_t /*syscall*/) {
    return "unknown";
}

/* ---- galloc.h ---- */
/* Declared as: void gm_stats(); */
void gm_stats() {}
