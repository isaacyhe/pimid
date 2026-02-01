/**
 * @file MachineID.hh
 * @brief Machine identifier for Ruby network
 */

#ifndef __GARNET_COMPAT_MEM_RUBY_COMMON_MACHINEID_HH__
#define __GARNET_COMPAT_MEM_RUBY_COMMON_MACHINEID_HH__

#include <cstdint>
#include <functional>
#include <iostream>

#include "TypeDefines.hh"

namespace gem5 {
namespace ruby {

/**
 * MachineID - identifies a machine in the Ruby network
 */
struct MachineID {
    MachineType type;
    uint32_t num;

    MachineID() : type(MachineType::NULL_MACHINE), num(0) {}
    MachineID(MachineType t, uint32_t n) : type(t), num(n) {}

    bool operator==(const MachineID& other) const {
        return type == other.type && num == other.num;
    }

    bool operator!=(const MachineID& other) const {
        return !(*this == other);
    }

    bool operator<(const MachineID& other) const {
        if (type != other.type)
            return static_cast<int>(type) < static_cast<int>(other.type);
        return num < other.num;
    }
};

inline std::ostream& operator<<(std::ostream& out, const MachineID& id) {
    out << MachineType_to_string(id.type) << "-" << id.num;
    return out;
}

} // namespace ruby
} // namespace gem5

// Hash function for MachineID
namespace std {
template <>
struct hash<gem5::ruby::MachineID> {
    size_t operator()(const gem5::ruby::MachineID& id) const {
        return hash<int>()(static_cast<int>(id.type)) ^
               (hash<uint32_t>()(id.num) << 1);
    }
};
} // namespace std

#endif // __GARNET_COMPAT_MEM_RUBY_COMMON_MACHINEID_HH__
