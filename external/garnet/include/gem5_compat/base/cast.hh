/**
 * @file cast.hh
 * @brief Safe cast utilities for Garnet
 */

#ifndef __GARNET_COMPAT_BASE_CAST_HH__
#define __GARNET_COMPAT_BASE_CAST_HH__

#include <cassert>
#include <type_traits>

namespace gem5 {

/**
 * Safe cast with runtime check (in debug builds)
 */
template <typename T, typename U>
inline T safe_cast(U&& ref_or_ptr) {
    // For pointers
    if constexpr (std::is_pointer_v<std::remove_reference_t<T>>) {
        auto* ptr = dynamic_cast<T>(ref_or_ptr);
        assert(ptr != nullptr && "safe_cast failed");
        return ptr;
    } else {
        // For references
        return dynamic_cast<T>(std::forward<U>(ref_or_ptr));
    }
}

} // namespace gem5

#endif // __GARNET_COMPAT_BASE_CAST_HH__
