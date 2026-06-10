/**
 * @file BasicRouter.hh
 * @brief Base router class for network
 */

#ifndef __GARNET_COMPAT_MEM_RUBY_NETWORK_BASICROUTER_HH__
#define __GARNET_COMPAT_MEM_RUBY_NETWORK_BASICROUTER_HH__

#include <string>
#include <vector>

#include "sim/clocked_object.hh"
#include "base/types.hh"

namespace gem5 {
namespace ruby {

// Forward declaration
class Network;

/**
 * BasicRouter - base class for network routers
 */
class BasicRouter : public ClockedObject {
public:
    struct Params : public ClockedObject::Params {
        uint32_t router_id = 0;
        Cycles latency = Cycles(1);
    };

    BasicRouter(const Params& p)
        : ClockedObject(p), routerId_(p.router_id), latency_(p.latency) {
        m_id = static_cast<int>(routerId_);
    }

    virtual ~BasicRouter() = default;

    // Router identification
    uint32_t get_id() const { return routerId_; }

    // Router latency
    Cycles get_latency() const { return latency_; }

    // Print for debugging
    virtual void print(std::ostream& out) const {
        out << "Router " << routerId_;
    }

    // Statistics registration (virtual for override)
    virtual void regStats() {}

protected:
    uint32_t routerId_;
    Cycles latency_;
    // Alias for gem5 compatibility
    int m_id = 0;  // Set in constructor
};

inline std::ostream& operator<<(std::ostream& out, const BasicRouter& router) {
    router.print(out);
    return out;
}

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_MEM_RUBY_NETWORK_BASICROUTER_HH__
