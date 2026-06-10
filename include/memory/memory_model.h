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
