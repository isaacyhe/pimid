/**
 * @file LinkDirection.hh
 * @brief Link direction enum (protocol stub)
 */

#ifndef __GARNET_COMPAT_MEM_RUBY_PROTOCOL_LINKDIRECTION_HH__
#define __GARNET_COMPAT_MEM_RUBY_PROTOCOL_LINKDIRECTION_HH__

namespace gem5 {
namespace ruby {

// LinkDirection as integer constants for use as array indices
// In gem5, these come from the SLICC-generated protocol
constexpr int LinkDirection_In = 0;
constexpr int LinkDirection_Out = 1;

// Enum for type safety when needed
enum class LinkDirection {
    In = LinkDirection_In,
    Out = LinkDirection_Out
};

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_MEM_RUBY_PROTOCOL_LINKDIRECTION_HH__
