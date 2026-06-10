/** Simplified BBL decoder implementation for QEMU-based instrumentation.
 *
 * Allocates BblInfo structures from the ZSim global heap. These are cached
 * by the QEMU plugin to avoid re-allocation for the same translation block.
 */

#include "decoder_simple.h"
#include "core.h"
#include "galloc.h"
#include <string.h>

BblInfo* createSimpleBblInfo(uint32_t instrs, uint32_t bytes) {
    // Allocate BblInfo + one DynBbl header so that oooBbl[0].uops is valid.
    // OOO/Timing cores access oooBbl[0] even for synthetic BBLs; without the
    // extra space, they read past the allocation (undefined behaviour / segfault).
    // gm_calloc zeros the memory, so oooBbl[0].uops == 0 automatically.
    size_t total = sizeof(BblInfo) + sizeof(DynBbl);
    BblInfo* bbl = static_cast<BblInfo*>(__gm_calloc(1, total));
    bbl->instrs = instrs;
    bbl->bytes = bytes;
    return bbl;
}
