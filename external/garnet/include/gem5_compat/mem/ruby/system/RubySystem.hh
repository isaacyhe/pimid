/**
 * @file RubySystem.hh
 * @brief Ruby system stub for standalone Garnet
 */

#ifndef __GARNET_COMPAT_MEM_RUBY_SYSTEM_RUBYSYSTEM_HH__
#define __GARNET_COMPAT_MEM_RUBY_SYSTEM_RUBYSYSTEM_HH__

#include "sim/clocked_object.hh"
#include "base/types.hh"
#include "../common/TypeDefines.hh"

namespace gem5 {
namespace ruby {

/**
 * RubySystem - stub for standalone Garnet
 *
 * In gem5, RubySystem manages the coherence protocol and memory system.
 * For standalone Garnet, we just need basic timing functionality.
 */
class RubySystem : public ClockedObject {
public:
    struct Params : public ClockedObject::Params {
        bool randomization = false;
        bool warmup_enabled = false;
    };

    RubySystem(const Params& p)
        : ClockedObject(p),
          randomization_(p.randomization),
          warmupEnabled_(p.warmup_enabled),
          startCycle_(Cycles(0)) {}

    virtual ~RubySystem() = default;

    // Start cycle tracking
    Cycles getStartCycle() const { return startCycle_; }
    void setStartCycle(Cycles c) { startCycle_ = c; }

    // Randomization control
    bool getRandomization() const { return randomization_; }
    void setRandomization(bool val) { randomization_ = val; }

    // Warmup control
    bool getWarmupEnabled() const { return warmupEnabled_; }
    void setWarmupEnabled(bool val) { warmupEnabled_ = val; }

    // Reset stats
    void resetStats() {
        // Override in derived class if needed
    }

    // Get base node number for a machine type
    // In full gem5, this depends on protocol configuration
    // For standalone Garnet, we use a simple scheme
    int MachineType_base_number(MachineType type) const {
        // Each machine type gets 256 slots
        return static_cast<int>(type) * 256;
    }

private:
    bool randomization_;
    bool warmupEnabled_;
    Cycles startCycle_;
};

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_MEM_RUBY_SYSTEM_RUBYSYSTEM_HH__
