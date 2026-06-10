/**
 * @file RubyNetwork.hh
 * @brief Debug flag for Ruby/Garnet network (stub)
 */

#ifndef __GARNET_COMPAT_DEBUG_RUBYNETWORK_HH__
#define __GARNET_COMPAT_DEBUG_RUBYNETWORK_HH__

#include "base/trace.hh"

// This is a stub - debug flags are handled by trace.hh
// The DPRINTF macro checks garnet_debug_enabled

namespace gem5 {
namespace debug {

// Debug flag structure (stub)
struct RubyNetwork {
    static constexpr bool enabled =
#ifdef GARNET_DEBUG
        true;
#else
        false;
#endif
};

} // namespace debug
} // namespace gem5

#endif // __GARNET_COMPAT_DEBUG_RUBYNETWORK_HH__
