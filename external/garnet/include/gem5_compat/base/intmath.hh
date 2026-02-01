/**
 * @file intmath.hh
 * @brief Integer math utilities for Garnet
 */

#ifndef __GARNET_COMPAT_BASE_INTMATH_HH__
#define __GARNET_COMPAT_BASE_INTMATH_HH__

#include <cstdint>
#include <type_traits>

namespace gem5 {

/**
 * Divide and round up (ceiling division)
 */
template <typename T, typename U>
constexpr auto divCeil(T a, U b) -> decltype(a / b) {
    return (a + b - 1) / b;
}

/**
 * Check if a number is a power of 2
 */
template <typename T>
constexpr bool isPowerOf2(T n) {
    static_assert(std::is_integral_v<T>, "isPowerOf2 requires integral type");
    return n > 0 && (n & (n - 1)) == 0;
}

/**
 * Calculate floor(log2(x))
 */
template <typename T>
constexpr int floorLog2(T x) {
    static_assert(std::is_integral_v<T>, "floorLog2 requires integral type");
    int y = 0;
    while (x > 1) {
        y++;
        x >>= 1;
    }
    return y;
}

/**
 * Calculate ceil(log2(x))
 */
template <typename T>
constexpr int ceilLog2(T x) {
    if (x <= 1) return 0;
    return floorLog2(x - 1) + 1;
}

/**
 * Round up to the nearest power of 2
 */
template <typename T>
constexpr T roundUpPow2(T x) {
    if (isPowerOf2(x)) return x;
    return T(1) << ceilLog2(x);
}

} // namespace gem5

#endif // __GARNET_COMPAT_BASE_INTMATH_HH__
