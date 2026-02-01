/**
 * @file TypeDefines.hh
 * @brief Type definitions for Ruby network
 */

#ifndef __GARNET_COMPAT_MEM_RUBY_COMMON_TYPEDEFINES_HH__
#define __GARNET_COMPAT_MEM_RUBY_COMMON_TYPEDEFINES_HH__

#include <string>

namespace gem5 {
namespace ruby {

// Port direction type
using PortDirection = std::string;

// Virtual network type
enum class VirtualNetworkType {
    DATA,
    CONTROL,
    RESPONSE,
    NUM_VN_TYPES
};

// Machine types (simplified)
enum class MachineType {
    L1Cache,
    L2Cache,
    Directory,
    DMA,
    Collector,
    NUM_MACHTYPES,
    NULL_MACHINE = -1
};

// Integer constant for iteration over machine types
constexpr int MachineType_NUM = static_cast<int>(MachineType::NUM_MACHTYPES);

// Message size types
enum class MessageSizeType {
    Control,
    Data,
    Request_Control,
    Response_Control,
    Response_Data,
    Writeback_Control,
    Writeback_Data,
    Broadcast_Control,
    Multicast_Control,
    Unblock_Control,
    Persistent_Control,
    Completion_Control,
    NUM_MESSAGE_SIZE_TYPES
};

// Convert machine type to string
inline std::string MachineType_to_string(MachineType type) {
    switch (type) {
        case MachineType::L1Cache: return "L1Cache";
        case MachineType::L2Cache: return "L2Cache";
        case MachineType::Directory: return "Directory";
        case MachineType::DMA: return "DMA";
        case MachineType::Collector: return "Collector";
        default: return "Unknown";
    }
}

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_MEM_RUBY_COMMON_TYPEDEFINES_HH__
