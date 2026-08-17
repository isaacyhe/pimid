#ifndef PIMID_MEMORY_MODEL_H
#define PIMID_MEMORY_MODEL_H

#include "common/types.h"
#include <string>

namespace pimid {

/**
 * Abstract base class for memory models
 * Provides standardized interface for different memory technologies
 */
class MemoryModel {
public:
    MemoryModel(MemoryTechnology tech, const std::string& config_path)
        : technology_(tech), config_path_(config_path) {}

    virtual ~MemoryModel() = default;

    // Initialization
    virtual void initialize() = 0;
    virtual void loadConfig(const std::string& config_path) = 0;

    // Memory operations
    virtual Cycle access(const MemoryRequest& req) = 0;
    virtual bool canAccept(const MemoryRequest& req) = 0;
    virtual void tick() = 0;

    // Energy modeling
    virtual double getReadEnergy() const = 0;
    virtual double getWriteEnergy() const = 0;
    virtual double getLeakagePower() const = 0;
    virtual double getTotalEnergy() const = 0;

    // Configuration queries
    virtual uint64_t getCapacity() const = 0;
    virtual uint64_t getBandwidth() const = 0;
    virtual Cycle getLatency(MemoryRequestType type) const = 0;

    // Statistics
    virtual void printStats() const = 0;
    virtual void resetStats() = 0;

    //=========================================================================
    // 1.11.24: THE MEMORY PLUGIN CONTRACT -- the uniform tier query.
    //
    // Every model already resolved subarray/bank/chip, but under different
    // names and arities: getSubarrayAccessLatency (DRAM) vs
    // getSubarrayReadLatency (SRAM/NVM), and PCM alone carries SET/RESET.
    // That divergence is why the factory was never wired: main.cpp could not
    // ask a MemoryModel for a placement latency without knowing which
    // technology it was talking to -- which defeats the point of a factory.
    //
    // One query, technology-agnostic. Adding a memory technology becomes
    // "implement this contract and bind your tool"; no main.cpp edit.
    //
    // hasTier() is load-bearing, not decoration: SRAM and NVM are NOT
    // DRAM-like. Bank groups and ranks collapse to 1 for them and no
    // architecture header even declares those fields, so a tier that does not
    // exist must be REPORTABLE AS ABSENT rather than silently collapsed onto
    // its neighbour or invented from a multiplier. That invention is exactly
    // what 1.11.23 found and removed.
    //
    // latencySourceNs() returns the provenance of the number, per tier and
    // per operation. A model that cannot source a tier returns a negative
    // latency and a reason; callers REFUSE rather than substitute. This is
    // the vendorArrayFraction() discipline applied to timing.
    //=========================================================================
    /* 1.11.25: the ARRAY the model must characterize. Without this a model
     * built through the factory uses its own compiled-in default -- STTMRAMModel
     * defaults to 256 MB -- while the live path deliberately characterizes the
     * 64 KB per-bank unit a PE actually owns ("each PE reads/writes its OWN
     * local 64KB bank"). Those differ by 148x in read latency on our own
     * cached characterization, so a tier ladder taken from an unconfigured
     * model describes a different device than the one being simulated.
     * 0 = leave the model's default. */
    virtual void setArrayCapacityBytes(uint64_t /*bytes*/) {}

    /* 1.11.25: and the ACCESS GEOMETRY. Capacity alone was not enough -- the
     * models default to a 64-bit word while the live path characterizes a
     * 512-bit access ("one full 64 B line per access, matching the CACTI/SRAM
     * path"), which selects a DIFFERENT pregenerated cache entry and a
     * different latency. Two knobs, one lesson: a model reached through the
     * factory must be configured identically to the path it replaces, or the
     * ladder describes a device nobody is simulating. 0 = keep the default. */
    virtual void setAccessWidthBits(uint32_t /*bits*/) {}

    /* 1.11.51 (L70, plugin leg): characterize at the RUN's technology node.
     * Each model shipped its own compiled-in default (SRAM/STT 22, ReRAM 32,
     * PCM 90 nm) that nothing ever set -- the per-model config-map knobs had
     * no writer -- so tier timing was priced at a node the energy path did
     * not use. Must precede initialize(). <=0 keeps the model default. */
    virtual void setTechNodeNm(int /*nm*/) {}

    enum class Tier { SUBARRAY, BANK, BANKGROUP, CHIP, RANK, CHANNEL };
    enum class Op   { READ, WRITE, SET, RESET };

    /* Latency in ns for one tier/op, or <0 when this model cannot source it.
     * NEVER returns a fabricated value: absence is reported, not filled. */
    virtual double getTierLatencyNs(Tier tier, Op op) const = 0;

    /* Does this technology physically have that tier? */
    virtual bool hasTier(Tier tier) const = 0;

    /* Where the number came from -- the tool and the quantity, e.g.
     * "NVSim bank->readLatency" or "Ramulator tRCD+tCAS". Empty when the
     * model cannot source it. */
    virtual std::string tierLatencySource(Tier tier, Op op) const = 0;

    static const char* tierName(Tier t) {
        switch (t) {
            case Tier::SUBARRAY:  return "subarray";
            case Tier::BANK:      return "bank";
            case Tier::BANKGROUP: return "bankgroup";
            case Tier::CHIP:      return "chip";
            case Tier::RANK:      return "rank";
            case Tier::CHANNEL:   return "channel";
        }
        return "?";
    }
    static const char* opName(Op o) {
        switch (o) {
            case Op::READ:  return "read";
            case Op::WRITE: return "write";
            case Op::SET:   return "set";
            case Op::RESET: return "reset";
        }
        return "?";
    }

    MemoryTechnology getTechnology() const { return technology_; }

protected:
    MemoryTechnology technology_;
    std::string config_path_;
};

/**
 * Factory for creating memory models based on technology type
 */
class MemoryModelFactory {
public:
    static std::shared_ptr<MemoryModel> createMemoryModel(
        MemoryTechnology tech,
        const std::string& config_path);
};

} // namespace pimid

#endif // PIMID_MEMORY_MODEL_H
