/**
 * @file compiler.hh
 * @brief Compiler-specific macros for Garnet
 */

#ifndef __GARNET_COMPAT_BASE_COMPILER_HH__
#define __GARNET_COMPAT_BASE_COMPILER_HH__

// Likely/unlikely branch hints
#if defined(__GNUC__) || defined(__clang__)
    #define GEM5_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define GEM5_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define GEM5_LIKELY(x)   (x)
    #define GEM5_UNLIKELY(x) (x)
#endif

// Deprecated attribute
#if defined(__GNUC__) || defined(__clang__)
    #define GEM5_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
    #define GEM5_DEPRECATED(msg)
#endif

// Unused variable suppression
#define GEM5_VAR_USED [[maybe_unused]]

// No return function
#if defined(__GNUC__) || defined(__clang__)
    #define GEM5_NO_RETURN __attribute__((noreturn))
#else
    #define GEM5_NO_RETURN [[noreturn]]
#endif

// Class member variable unused suppression
#define GEM5_CLASS_VAR_USED [[maybe_unused]]

#endif // __GARNET_COMPAT_BASE_COMPILER_HH__
