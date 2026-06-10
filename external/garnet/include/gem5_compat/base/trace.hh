/**
 * @file trace.hh
 * @brief Debug/trace macros for Garnet (standalone implementation)
 */

#ifndef __GARNET_COMPAT_BASE_TRACE_HH__
#define __GARNET_COMPAT_BASE_TRACE_HH__

#include <cstdio>
#include <cstdlib>
#include <string>

namespace gem5 {

// Global debug flag - can be enabled at runtime
inline bool garnet_debug_enabled = false;

// Enable Garnet debug output
inline void enableGarnetDebug() { garnet_debug_enabled = true; }
inline void disableGarnetDebug() { garnet_debug_enabled = false; }

} // namespace gem5

// Debug print macro - can be disabled by not defining GARNET_DEBUG
// Note: In standalone mode, we just discard the flag parameter
#ifdef GARNET_DEBUG
    #define DPRINTF(flag, ...) \
        do { \
            if (gem5::garnet_debug_enabled) { \
                fprintf(stderr, "[Garnet] "); \
                fprintf(stderr, __VA_ARGS__); \
            } \
        } while (0)
#else
    #define DPRINTF(flag, ...) do {} while (0)
#endif

// Panic macro - fatal error
#define panic(...) \
    do { \
        fprintf(stderr, "PANIC: "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        abort(); \
    } while (0)

// Panic if condition is true
#define panic_if(cond, ...) \
    do { \
        if (cond) { \
            fprintf(stderr, "PANIC: "); \
            fprintf(stderr, __VA_ARGS__); \
            fprintf(stderr, "\n"); \
            abort(); \
        } \
    } while (0)

// Fatal macro
#define fatal(...) \
    do { \
        fprintf(stderr, "FATAL: "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        abort(); \
    } while (0)

// Fatal if condition is true
#define fatal_if(cond, ...) \
    do { \
        if (cond) { \
            fprintf(stderr, "FATAL: "); \
            fprintf(stderr, __VA_ARGS__); \
            fprintf(stderr, "\n"); \
            abort(); \
        } \
    } while (0)

// Warning macro
#define warn(...) \
    do { \
        fprintf(stderr, "WARNING: "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } while (0)

// Info macro
#define inform(...) \
    do { \
        fprintf(stderr, "INFO: "); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } while (0)

#endif // __GARNET_COMPAT_BASE_TRACE_HH__
