/**
 * @file NetDest.hh
 * @brief Network destination set for routing
 */

#ifndef __GARNET_COMPAT_MEM_RUBY_COMMON_NETDEST_HH__
#define __GARNET_COMPAT_MEM_RUBY_COMMON_NETDEST_HH__

#include <bitset>
#include <iostream>
#include <set>
#include <vector>

#include "MachineID.hh"
#include "TypeDefines.hh"
#include "base/types.hh"

namespace gem5 {
namespace ruby {

// Maximum number of nodes supported
constexpr int MAX_NODES = 1024;

/**
 * NetDest - represents a set of network destinations
 *
 * Used for routing and multicast operations.
 */
// Forward declaration
class RubySystem;

class NetDest {
public:
    NetDest() { clear(); }

    // Constructor that takes RubySystem* (for gem5 compatibility)
    explicit NetDest(RubySystem* /*rs*/) { clear(); }

    // Add a node to the destination set
    void add(NodeID node) {
        if (node < MAX_NODES)
            nodes_.set(node);
    }

    void add(MachineID id) {
        // Simplified: use machine num as node ID
        add(id.num);
    }

    // Remove a node from the set
    void remove(NodeID node) {
        if (node < MAX_NODES)
            nodes_.reset(node);
    }

    void remove(MachineID id) {
        remove(id.num);
    }

    // Remove all nodes in another NetDest
    void removeNetDest(const NetDest& other) {
        nodes_ &= ~other.nodes_;
    }

    // Clear all destinations
    void clear() {
        nodes_.reset();
    }

    // Check if a node is in the set
    bool contains(NodeID node) const {
        return node < MAX_NODES && nodes_.test(node);
    }

    bool contains(MachineID id) const {
        return contains(id.num);
    }

    // Check if set is empty
    bool isEmpty() const {
        return nodes_.none();
    }

    // Check if this is a subset of other
    bool isSubset(const NetDest& other) const {
        return (nodes_ & ~other.nodes_).none();
    }

    // Check if this intersects with other
    bool intersects(const NetDest& other) const {
        return (nodes_ & other.nodes_).any();
    }

    // Alias for intersects (gem5 naming)
    bool intersectionIsNotEmpty(const NetDest& other) const {
        return intersects(other);
    }

    // Count of destinations
    int count() const {
        return static_cast<int>(nodes_.count());
    }

    // Get all node IDs in the set
    std::vector<NodeID> getAllDest() const {
        std::vector<NodeID> result;
        for (size_t i = 0; i < MAX_NODES; i++) {
            if (nodes_.test(i))
                result.push_back(static_cast<NodeID>(i));
        }
        return result;
    }

    // Get first element (for unicast)
    NodeID smallestElement() const {
        for (size_t i = 0; i < MAX_NODES; i++) {
            if (nodes_.test(i))
                return static_cast<NodeID>(i);
        }
        return static_cast<NodeID>(-1);
    }

    // Set operations
    NetDest operator|(const NetDest& other) const {
        NetDest result;
        result.nodes_ = nodes_ | other.nodes_;
        return result;
    }

    NetDest operator&(const NetDest& other) const {
        NetDest result;
        result.nodes_ = nodes_ & other.nodes_;
        return result;
    }

    NetDest& operator|=(const NetDest& other) {
        nodes_ |= other.nodes_;
        return *this;
    }

    NetDest& operator&=(const NetDest& other) {
        nodes_ &= other.nodes_;
        return *this;
    }

    bool operator==(const NetDest& other) const {
        return nodes_ == other.nodes_;
    }

    bool operator!=(const NetDest& other) const {
        return nodes_ != other.nodes_;
    }

    // Print for debugging
    void print(std::ostream& out) const {
        out << "NetDest: {";
        bool first = true;
        for (size_t i = 0; i < MAX_NODES; i++) {
            if (nodes_.test(i)) {
                if (!first) out << ", ";
                out << i;
                first = false;
            }
        }
        out << "}";
    }

private:
    std::bitset<MAX_NODES> nodes_;
};

inline std::ostream& operator<<(std::ostream& out, const NetDest& dest) {
    dest.print(out);
    return out;
}

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_MEM_RUBY_COMMON_NETDEST_HH__
