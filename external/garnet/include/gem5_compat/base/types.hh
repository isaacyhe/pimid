/**
 * @file types.hh
 * @brief Standalone gem5 type definitions for Garnet extraction
 *
 * This provides the minimal type definitions needed by Garnet
 * without the full gem5 infrastructure.
 */

#ifndef __GARNET_COMPAT_BASE_TYPES_HH__
#define __GARNET_COMPAT_BASE_TYPES_HH__

#include <cstdint>
#include <limits>

namespace gem5 {

// Statistics counter type
typedef int64_t Counter;

// Tick count type (simulation time in picoseconds typically)
typedef uint64_t Tick;

const Tick MaxTick = std::numeric_limits<uint64_t>::max();

/**
 * Cycles wrapper class for type-safe cycle counting
 */
class Cycles {
private:
    uint64_t c;

public:
    explicit constexpr Cycles(uint64_t _c) : c(_c) {}
    Cycles() : c(0) {}

    constexpr operator uint64_t() const { return c; }

    Cycles& operator++() { ++c; return *this; }
    Cycles operator++(int) { Cycles old = *this; ++c; return old; }
    Cycles& operator--() { --c; return *this; }

    Cycles& operator+=(const Cycles& rhs) { c += rhs.c; return *this; }
    Cycles& operator-=(const Cycles& rhs) { c -= rhs.c; return *this; }

    bool operator==(const Cycles& rhs) const { return c == rhs.c; }
    bool operator!=(const Cycles& rhs) const { return c != rhs.c; }
    bool operator<(const Cycles& rhs) const { return c < rhs.c; }
    bool operator<=(const Cycles& rhs) const { return c <= rhs.c; }
    bool operator>(const Cycles& rhs) const { return c > rhs.c; }
    bool operator>=(const Cycles& rhs) const { return c >= rhs.c; }
};

inline Cycles operator+(const Cycles& a, const Cycles& b) {
    return Cycles(uint64_t(a) + uint64_t(b));
}

inline Cycles operator-(const Cycles& a, const Cycles& b) {
    return Cycles(uint64_t(a) - uint64_t(b));
}

// Node and switch identifiers
typedef uint32_t NodeID;
typedef uint32_t SwitchID;

// Invalid port ID
const int InvalidPortID = -1;

// Address type
typedef uint64_t Addr;

// Utility function for ceiling division
template <typename T>
constexpr T divCeil(T a, T b) {
    return (a + b - 1) / b;
}

} // namespace gem5

#endif // __GARNET_COMPAT_BASE_TYPES_HH__
