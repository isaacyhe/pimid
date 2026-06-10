/** ZSim type definitions for use without Intel Pin.
 *
 * When building with ZSIM_USE_QEMU, this header provides the type aliases
 * that ZSim's simulation core (cores, caches, scheduler) expects from Pin.
 * These are simple integer typedefs — no Pin functionality is needed.
 */

#ifndef ZSIM_TYPES_H_
#define ZSIM_TYPES_H_

#include <stdint.h>

// Core Pin types used throughout ZSim simulation code
typedef uint32_t THREADID;
typedef uint64_t ADDRINT;
typedef uint32_t BOOL;

// Address type used by ZSim's memory hierarchy
typedef uint64_t Address;

// REG_LAST is used by decoder.h for temporary register numbering.
// The actual value doesn't matter for SimpleCore (no OOO decoding).
// We set it high enough to avoid collisions with temp register offsets.
#define REG_LAST 512

// Boolean constants
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#endif  // ZSIM_TYPES_H_
