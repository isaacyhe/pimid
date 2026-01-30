/**
 * @file external_model_interface.h
 * @brief Lightweight interface for integrating external network and memory models
 *
 * DESIGN PHILOSOPHY:
 * - Users do NOT modify their existing models
 * - Users provide a thin adapter (config file + wrapper functions)
 * - PIMID loads models dynamically at runtime
 * - Simple C-style interface for maximum compatibility
 */

#ifndef PIMID_EXTERNAL_MODEL_INTERFACE_H
#define PIMID_EXTERNAL_MODEL_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Model Type Identification
//=============================================================================

typedef enum {
    PIMID_MODEL_NETWORK,
    PIMID_MODEL_MEMORY,
    PIMID_MODEL_POWER,
    PIMID_MODEL_THERMAL
} PimidModelType;

//=============================================================================
// Generic Model Handle (Opaque Pointer)
//=============================================================================

/**
 * @brief Opaque handle to user's model instance
 *
 * This can point to ANY user-defined structure.
 * PIMID never dereferences this - only passes it back to user callbacks.
 */
typedef void* PimidModelHandle;

//=============================================================================
// Network Model Interface (Minimal)
//=============================================================================

/**
 * @brief Network packet structure (PIMID's internal format)
 */
typedef struct {
    uint32_t src_id;
    uint32_t dst_id;
    uint64_t size_bytes;
    uint64_t addr;
    void* user_data;  // User can attach custom data
} PimidNetworkPacket;

/**
 * @brief Network statistics structure
 */
typedef struct {
    uint64_t total_packets;
    uint64_t total_bytes;
    uint64_t total_cycles;
    double avg_latency;
    double bandwidth_utilization;
} PimidNetworkStats;

/**
 * @brief Network model callback functions
 *
 * User provides these functions in their adapter.
 * Each function receives the model handle as first parameter.
 */
typedef struct {
    /** Initialize the model */
    int (*init)(PimidModelHandle handle, const char* config_file);

    /** Send a packet through the network */
    int (*send_packet)(PimidModelHandle handle, const PimidNetworkPacket* packet);

    /** Advance simulation by one cycle */
    void (*tick)(PimidModelHandle handle);

    /** Query latency for a transfer */
    uint64_t (*get_latency)(PimidModelHandle handle, uint32_t src, uint32_t dst, uint64_t bytes);

    /** Get current statistics */
    void (*get_stats)(PimidModelHandle handle, PimidNetworkStats* stats);

    /** Cleanup and destroy model */
    void (*destroy)(PimidModelHandle handle);
} PimidNetworkCallbacks;

//=============================================================================
// Memory Model Interface (Minimal)
//=============================================================================

/**
 * @brief Memory request structure
 */
typedef struct {
    uint64_t addr;
    uint64_t size_bytes;
    bool is_write;
    uint64_t cycle;
    void* user_data;
} PimidMemoryRequest;

/**
 * @brief Memory statistics structure
 */
typedef struct {
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t total_cycles;
    double avg_read_latency;
    double avg_write_latency;
    double bandwidth_utilization;
} PimidMemoryStats;

/**
 * @brief Memory model callback functions
 */
typedef struct {
    /** Initialize the model */
    int (*init)(PimidModelHandle handle, const char* config_file);

    /** Send a memory request */
    int (*send_request)(PimidModelHandle handle, const PimidMemoryRequest* req);

    /** Check if request completed */
    bool (*is_ready)(PimidModelHandle handle, const PimidMemoryRequest* req);

    /** Advance simulation by one cycle */
    void (*tick)(PimidModelHandle handle);

    /** Query latency for an access */
    uint64_t (*get_latency)(PimidModelHandle handle, uint64_t addr, uint64_t bytes, bool is_write);

    /** Get current statistics */
    void (*get_stats)(PimidModelHandle handle, PimidMemoryStats* stats);

    /** Cleanup and destroy model */
    void (*destroy)(PimidModelHandle handle);
} PimidMemoryCallbacks;

//=============================================================================
// Model Registration
//=============================================================================

/**
 * @brief Model descriptor (provided by user)
 */
typedef struct {
    const char* name;           // Model name (e.g., "BookSim", "DRAMSim3")
    const char* version;        // Version string
    PimidModelType type;        // Model type
    PimidModelHandle handle;    // User's model instance handle

    // Callbacks (set based on model type)
    union {
        PimidNetworkCallbacks network;
        PimidMemoryCallbacks memory;
    } callbacks;
} PimidModelDescriptor;

/**
 * @brief Register an external model with PIMID
 *
 * @param descriptor Model descriptor with callbacks
 * @return 0 on success, -1 on error
 */
int pimid_register_model(const PimidModelDescriptor* descriptor);

/**
 * @brief Unregister a model
 *
 * @param name Model name
 * @return 0 on success, -1 on error
 */
int pimid_unregister_model(const char* name);

/**
 * @brief Get a registered model by name
 *
 * @param name Model name
 * @return Model descriptor or NULL if not found
 */
const PimidModelDescriptor* pimid_get_model(const char* name);

//=============================================================================
// Dynamic Loading Support (Optional)
//=============================================================================

/**
 * @brief Entry point function for dynamically loaded models
 *
 * User's shared library (.so/.dll) should export this function:
 *
 * PimidModelDescriptor* pimid_model_create() {
 *     static PimidModelDescriptor desc;
 *     desc.name = "MyModel";
 *     desc.version = "1.0";
 *     // ... fill in callbacks ...
 *     return &desc;
 * }
 */
typedef PimidModelDescriptor* (*PimidModelCreateFunc)(void);

/**
 * @brief Load a model from a shared library
 *
 * @param library_path Path to .so/.dll file
 * @return 0 on success, -1 on error
 */
int pimid_load_model(const char* library_path);

#ifdef __cplusplus
}
#endif

#endif // PIMID_EXTERNAL_MODEL_INTERFACE_H
