/**
 * @file BasicLink.hh
 * @brief Base link class for network
 */

#ifndef __GARNET_COMPAT_MEM_RUBY_NETWORK_BASICLINK_HH__
#define __GARNET_COMPAT_MEM_RUBY_NETWORK_BASICLINK_HH__

#include <string>

#include "sim/clocked_object.hh"
#include "base/types.hh"

namespace gem5 {
namespace ruby {

/**
 * BasicLink - base class for network links
 */
class BasicLink : public ClockedObject {
public:
    struct Params : public ClockedObject::Params {
        Cycles latency = Cycles(1);
        int bandwidth_factor = 16;  // Link width in bytes
        int weight = 1;             // Routing weight
    };

    BasicLink(const Params& p)
        : ClockedObject(p),
          latency_(p.latency),
          bandwidthFactor_(p.bandwidth_factor),
          m_weight(p.weight) {}

    virtual ~BasicLink() = default;

    Cycles get_latency() const { return latency_; }
    int get_bandwidth_factor() const { return bandwidthFactor_; }
    int get_weight() const { return m_weight; }

    virtual void print(std::ostream& out) const {
        out << "Link [lat=" << latency_ << ", bw=" << bandwidthFactor_ << "]";
    }

protected:
    Cycles latency_;
    int bandwidthFactor_;
    int m_weight;  // gem5 naming for compatibility
};

/**
 * BasicExtLink - external link (router to NI)
 */
class BasicExtLink : public BasicLink {
public:
    struct Params : public BasicLink::Params {
        NodeID ext_node = 0;
        uint32_t int_node = 0;  // Router ID
    };

    BasicExtLink(const Params& p)
        : BasicLink(p), extNode_(p.ext_node), intNode_(p.int_node) {}

    NodeID getExtNode() const { return extNode_; }
    uint32_t getIntNode() const { return intNode_; }

protected:
    NodeID extNode_;
    uint32_t intNode_;
};

/**
 * BasicIntLink - internal link (router to router)
 */
class BasicIntLink : public BasicLink {
public:
    struct Params : public BasicLink::Params {
        uint32_t src_node = 0;
        uint32_t dst_node = 0;
        std::string src_outport;
        std::string dst_inport;
    };

    BasicIntLink(const Params& p)
        : BasicLink(p),
          srcNode_(p.src_node),
          dstNode_(p.dst_node),
          srcOutport_(p.src_outport),
          dstInport_(p.dst_inport) {}

    uint32_t getSrcNode() const { return srcNode_; }
    uint32_t getDstNode() const { return dstNode_; }
    const std::string& getSrcOutport() const { return srcOutport_; }
    const std::string& getDstInport() const { return dstInport_; }

protected:
    uint32_t srcNode_;
    uint32_t dstNode_;
    std::string srcOutport_;
    std::string dstInport_;
};

inline std::ostream& operator<<(std::ostream& out, const BasicLink& link) {
    link.print(out);
    return out;
}

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_MEM_RUBY_NETWORK_BASICLINK_HH__
