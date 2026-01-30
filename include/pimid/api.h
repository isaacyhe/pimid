/**
 * @file api.h
 * @brief PIMID Public API for Workload Integration
 *
 * This header defines the public API that workloads can use to interact
 * with the PIMID simulator. Workloads can be written in C/C++ and use
 * standard parallel programming models (OpenMP, MPI) alongside PIMID-specific
 * annotations for PIM regions.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

//=============================================================================
// PIMID Context and Initialization
//=============================================================================

/**
 * @brief Opaque handle to PIMID simulation context
 */
typedef struct pimid_context* pimid_context_t;

/**
 * @brief Initialize PIMID context
 *
 * This should be called at the beginning of the workload.
 * When running outside PIMID, this is a no-op.
 *
 * @return PIMID context handle, or NULL if not running under PIMID
 */
pimid_context_t pimid_init(void);

/**
 * @brief Finalize PIMID context and output statistics
 *
 * @param ctx PIMID context handle
 */
void pimid_finalize(pimid_context_t ctx);

/**
 * @brief Check if running under PIMID simulator
 *
 * @return 1 if running under PIMID, 0 otherwise
 */
int pimid_is_active(void);

//=============================================================================
// PIM Region Annotations
//=============================================================================

/**
 * @brief Mark the beginning of a PIM-offloadable region
 *
 * Code between PIMID_BEGIN_PIM_REGION and PIMID_END_PIM_REGION will be
 * simulated on the configured PIM architecture.
 *
 * @param region_name Name/description of this PIM region
 */
void pimid_begin_pim_region(const char* region_name);

/**
 * @brief Mark the end of a PIM region
 */
void pimid_end_pim_region(void);

/**
 * @brief Macro for PIM region start (can be no-op when not under PIMID)
 */
#ifdef PIMID_ENABLED
#define PIMID_BEGIN_PIM_REGION(name) pimid_begin_pim_region(name)
#define PIMID_END_PIM_REGION() pimid_end_pim_region()
#else
#define PIMID_BEGIN_PIM_REGION(name) do {} while(0)
#define PIMID_END_PIM_REGION() do {} while(0)
#endif

//=============================================================================
// Memory Management
//=============================================================================

/**
 * @brief Allocate memory that will be tracked by PIMID
 *
 * Similar to malloc, but memory accesses will be tracked and simulated.
 *
 * @param size Size in bytes
 * @return Pointer to allocated memory, or NULL on failure
 */
void* pimid_malloc(size_t size);

/**
 * @brief Allocate aligned memory tracked by PIMID
 *
 * @param alignment Alignment requirement (must be power of 2)
 * @param size Size in bytes
 * @return Pointer to allocated memory, or NULL on failure
 */
void* pimid_aligned_alloc(size_t alignment, size_t size);

/**
 * @brief Free memory allocated with pimid_malloc
 *
 * @param ptr Pointer to memory to free
 */
void pimid_free(void* ptr);

//=============================================================================
// Memory Hints and Attributes
//=============================================================================

/**
 * @brief Memory access pattern hints
 */
typedef enum {
    PIMID_ACCESS_RANDOM = 0,      ///< Random access pattern
    PIMID_ACCESS_SEQUENTIAL,      ///< Sequential access
    PIMID_ACCESS_STRIDED,         ///< Strided access
    PIMID_ACCESS_GATHER_SCATTER   ///< Irregular gather/scatter
} pimid_access_pattern_t;

/**
 * @brief Provide hint about memory access pattern
 *
 * This helps the simulator optimize memory modeling.
 *
 * @param ptr Memory region base address
 * @param size Size of memory region
 * @param pattern Access pattern hint
 */
void pimid_hint_access_pattern(void* ptr, size_t size, pimid_access_pattern_t pattern);

/**
 * @brief Mark memory as read-only in PIM region
 *
 * @param ptr Memory region base address
 * @param size Size of memory region
 */
void pimid_mark_readonly(void* ptr, size_t size);

/**
 * @brief Mark memory as write-only in PIM region
 *
 * @param ptr Memory region base address
 * @param size Size of memory region
 */
void pimid_mark_writeonly(void* ptr, size_t size);

//=============================================================================
// PIM Operations
//=============================================================================

/**
 * @brief Offload a computation kernel to PIM
 *
 * This is a high-level API for explicit PIM offload.
 *
 * @param kernel_name Name of the kernel (for statistics)
 * @param data Input/output data pointer
 * @param size Data size in bytes
 * @param kernel_func Function pointer to kernel (optional, can be NULL)
 * @param args Kernel arguments (optional)
 */
void pimid_offload(const char* kernel_name, void* data, size_t size,
                   void (*kernel_func)(void*), void* args);

//=============================================================================
// Statistics and Profiling
//=============================================================================

/**
 * @brief Get current simulation cycle count
 *
 * @return Current cycle count
 */
uint64_t pimid_get_cycles(void);

/**
 * @brief Get current simulation time in nanoseconds
 *
 * @return Current simulation time (ns)
 */
double pimid_get_time_ns(void);

/**
 * @brief Get energy consumption so far (picojoules)
 *
 * @return Total energy consumed (pJ)
 */
double pimid_get_energy_pj(void);

/**
 * @brief Print statistics for a specific region
 *
 * @param region_name Name of region to report
 */
void pimid_print_region_stats(const char* region_name);

/**
 * @brief Reset statistics counters
 */
void pimid_reset_stats(void);

//=============================================================================
// Configuration Query
//=============================================================================

/**
 * @brief Get number of PIM processing elements
 *
 * @return Number of PEs in current configuration
 */
int pimid_get_num_pes(void);

/**
 * @brief Get memory technology name
 *
 * @return String describing memory technology (e.g., "SRAM", "DRAM")
 */
const char* pimid_get_memory_tech(void);

/**
 * @brief Get PE placement level
 *
 * @return String describing placement (e.g., "BANK", "SUBARRAY")
 */
const char* pimid_get_placement_level(void);

//=============================================================================
// Workload Entry Point
//=============================================================================

/**
 * @brief Standard entry point for PIMID workloads
 *
 * Workloads should implement this function instead of main() when
 * they want to be loaded dynamically by PIMID.
 *
 * When compiling as standalone executable, provide a regular main()
 * that calls workload_main().
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @param ctx PIMID context (provided by simulator)
 * @return Exit code
 */
int workload_main(int argc, char** argv, pimid_context_t ctx);

#ifdef __cplusplus
}
#endif

//=============================================================================
// C++ Convenience Wrappers
//=============================================================================

#ifdef __cplusplus
namespace pimid {

/**
 * @brief RAII wrapper for PIM regions
 */
class PIMRegion {
public:
    explicit PIMRegion(const char* name) : name_(name) {
        pimid_begin_pim_region(name);
    }

    ~PIMRegion() {
        pimid_end_pim_region();
    }

    // Non-copyable
    PIMRegion(const PIMRegion&) = delete;
    PIMRegion& operator=(const PIMRegion&) = delete;

private:
    const char* name_;
};

/**
 * @brief Smart pointer for PIMID-tracked memory
 */
template<typename T>
class tracked_ptr {
public:
    tracked_ptr() : ptr_(nullptr) {}

    explicit tracked_ptr(size_t count) {
        ptr_ = static_cast<T*>(pimid_malloc(count * sizeof(T)));
    }

    ~tracked_ptr() {
        if (ptr_) {
            pimid_free(ptr_);
        }
    }

    // Move semantics
    tracked_ptr(tracked_ptr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    tracked_ptr& operator=(tracked_ptr&& other) noexcept {
        if (this != &other) {
            if (ptr_) pimid_free(ptr_);
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    // Non-copyable
    tracked_ptr(const tracked_ptr&) = delete;
    tracked_ptr& operator=(const tracked_ptr&) = delete;

    T* get() { return ptr_; }
    const T* get() const { return ptr_; }

    T& operator[](size_t idx) { return ptr_[idx]; }
    const T& operator[](size_t idx) const { return ptr_[idx]; }

    operator T*() { return ptr_; }
    operator const T*() const { return ptr_; }

private:
    T* ptr_;
};

} // namespace pimid
#endif
