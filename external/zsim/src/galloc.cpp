/** $lic$
 * Copyright (C) 2012-2015 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2013 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * zsim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2013) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "galloc.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

// Direct syscall wrappers for SysV shared memory
// Pin 4.x's pinrt/musl might not implement these, so we call the kernel directly
#ifdef __x86_64__
static inline void* syscall_shmat(int shmid, const void* shmaddr, int shmflg) {
    void* ret;
    long result = syscall(SYS_shmat, shmid, shmaddr, shmflg);
    if (result < 0 && result > -4096) {
        errno = -result;
        ret = (void*)-1;
    } else {
        ret = (void*)result;
    }
    return ret;
}

static inline int syscall_shmdt(const void* shmaddr) {
    return syscall(SYS_shmdt, shmaddr);
}
#else
// Fallback to libc for non-x86_64
#define syscall_shmat shmat
#define syscall_shmdt shmdt
#endif

#include "log.h"  // NOLINT must precede dlmalloc, which defines assert if undefined
#include "g_heap/dlmalloc.h.c"
#include "locks.h"
#include "pad.h"

/* Base heap address. Has to be available cross-process. With 64-bit virtual
 * addresses, the address space is so sparse that it's quite easy to find
 * some random base that always works in practice. If for some weird reason
 * you want to compile this on a 32-bit address space, there are fancier,
 * more structured ways to get a common range (e.g. launch all the processes
 * before allocating the global heap segment, and find a common range either
 * by brute-force scanning and communicating through pipes, or by parsing
 * /proc/{pid}/maps).
 *
 * But, since I'm using a 64-bit address space, I don't really care to make
 * it fancy.
 */
// Base heap address - must be available in all processes
#ifndef GM_BASE_ADDR
#define GM_BASE_ADDR ((void*)0x00ABBA000000)
#endif

struct gm_segment {
    volatile void* base_regp; //common data structure, accessible with glob_ptr; threads poll on gm_isready to determine when everything has been initialized
    volatile void* secondary_regp; //secondary data structure, used to exchange information between harness and initializing process
    mspace mspace_ptr;

    PAD();
    lock_t lock;
    PAD();
};

static gm_segment* GM = nullptr;
static int gm_shmid = 0;
static bool gm_use_posix = false;  // True if using POSIX shared memory instead of SysV
static char gm_shm_name[64] = {0}; // POSIX shared memory name
static size_t gm_segment_size = 0; // Segment size for POSIX mmap
static intptr_t gm_reloc_offset = 0; // Relocation offset for non-fixed address mapping

/* Try POSIX shared memory as fallback for sandboxed environments
 *
 * This uses non-fixed address mapping to support environments where
 * MAP_FIXED is blocked (like Pin 4.x instrumented processes).
 *
 * Strategy:
 * 1. Harness creates shm with mmap (no MAP_FIXED) - OS chooses address
 * 2. Harness stores the base address in environment variable
 * 3. Children try to mmap at the same address (hint, not MAP_FIXED)
 * 4. If children get the same address, no pointer relocation needed
 * 5. If children get a different address, apply pointer relocation
 */
static bool gm_try_posix_create(size_t segmentSize) {
    // Generate a unique name based on PID
    snprintf(gm_shm_name, sizeof(gm_shm_name), "/zsim_gm_%d", getpid());

    // Remove any existing segment with the same name
    shm_unlink(gm_shm_name);

    int fd = shm_open(gm_shm_name, O_CREAT | O_RDWR | O_EXCL, 0644);
    if (fd == -1) {
        warn("POSIX shm_open failed: errno=%d", errno);
        return false;
    }

    if (ftruncate(fd, segmentSize) == -1) {
        warn("POSIX ftruncate failed: errno=%d", errno);
        close(fd);
        shm_unlink(gm_shm_name);
        return false;
    }

    // For POSIX shared memory mode, we skip MAP_FIXED entirely.
    // This allows children (running under Pin which blocks MAP_FIXED)
    // to potentially map at the same OS-chosen address as the harness.
    //
    // The idea is:
    // 1. Harness creates shm and maps without MAP_FIXED
    // 2. OS chooses an address for harness (e.g., 0x7fff...)
    // 3. Harness passes this address to children via env var
    // 4. Children try to map at the same address (as hint)
    // 5. If successful (same address), no pointer relocation needed

    // Use NULL hint - let OS choose the address
    GM = static_cast<gm_segment*>(mmap(nullptr, segmentSize,
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED, fd, 0));
    close(fd);

    if (GM == MAP_FAILED) {
        warn("POSIX mmap failed: errno=%d", errno);
        shm_unlink(gm_shm_name);
        GM = nullptr;
        return false;
    }

    gm_use_posix = true;
    gm_segment_size = segmentSize;
    gm_reloc_offset = 0;  // Harness is the reference - no relocation needed

    // Store the actual mapped address for children to use as a hint
    char addr_str[32];
    snprintf(addr_str, sizeof(addr_str), "%p", GM);
    setenv("ZSIM_POSIX_SHM_ADDR", addr_str, 1);

    info("Using POSIX shared memory: %s -> %p", gm_shm_name, GM);
    setenv("ZSIM_POSIX_SHM", gm_shm_name, 1);
    return true;
}

static bool gm_try_posix_attach(const char* shm_name) {
    int fd = shm_open(shm_name, O_RDWR, 0644);
    if (fd == -1) {
        warn("POSIX shm_open (attach) failed: errno=%d", errno);
        return false;
    }

    // Get the size from the existing segment
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        warn("POSIX fstat failed: errno=%d", errno);
        close(fd);
        return false;
    }
    size_t segmentSize = sb.st_size;

    // Get the harness's mapped address from environment (if available)
    void* harness_addr = nullptr;
    const char* addr_str = getenv("ZSIM_POSIX_SHM_ADDR");
    if (addr_str) {
        harness_addr = (void*)strtoull(addr_str, nullptr, 0);
        info("Harness mapped at %p, will try to map there", harness_addr);
    }

    // First try the harness's address as a hint (not MAP_FIXED)
    // If the address is available, we'll get it and avoid relocation
    if (harness_addr) {
        GM = static_cast<gm_segment*>(mmap(harness_addr, segmentSize,
                                            PROT_READ | PROT_WRITE,
                                            MAP_SHARED, fd, 0));
        if (GM == harness_addr) {
            close(fd);
            strncpy(gm_shm_name, shm_name, sizeof(gm_shm_name) - 1);
            gm_use_posix = true;
            gm_segment_size = segmentSize;
            gm_reloc_offset = 0;
            info("POSIX mmap: Attached at harness address %p - no relocation needed", GM);
            return true;
        }
        // Hint was not honored - unmap and try again
        if (GM != MAP_FAILED) {
            munmap(GM, segmentSize);
        }
    }

    // Try fixed address (legacy behavior) - will likely fail in Pin 4.x
    GM = static_cast<gm_segment*>(mmap(GM_BASE_ADDR, segmentSize,
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED | MAP_FIXED, fd, 0));
    if (GM == GM_BASE_ADDR) {
        close(fd);
        strncpy(gm_shm_name, shm_name, sizeof(gm_shm_name) - 1);
        gm_use_posix = true;
        gm_segment_size = segmentSize;
        gm_reloc_offset = 0;
        info("POSIX mmap: Attached at fixed address %p", GM);
        return true;
    }

    // Fixed address failed - try without any hint
    if (GM != MAP_FAILED) {
        munmap(GM, segmentSize);
    }

    GM = static_cast<gm_segment*>(mmap(nullptr, segmentSize,
                                        PROT_READ | PROT_WRITE,
                                        MAP_SHARED, fd, 0));
    close(fd);

    if (GM == MAP_FAILED) {
        warn("POSIX mmap (attach) failed: errno=%d", errno);
        GM = nullptr;
        return false;
    }

    // Calculate relocation offset relative to harness's address (or GM_BASE_ADDR)
    void* base_addr = harness_addr ? harness_addr : GM_BASE_ADDR;
    gm_reloc_offset = (intptr_t)GM - (intptr_t)base_addr;
    info("POSIX mmap: Attached at %p instead of %p (reloc_offset=%ld)",
         GM, base_addr, gm_reloc_offset);

    strncpy(gm_shm_name, shm_name, sizeof(gm_shm_name) - 1);
    gm_use_posix = true;
    gm_segment_size = segmentSize;
    return true;
}

/* Heap segment size, in bytes. Can't grow for now, so choose something sensible, and within the machine's limits (see sysctl vars kernel.shmmax and kernel.shmall) */
int gm_init(size_t segmentSize) {
    /* Create a SysV IPC shared memory segment, attach to it, and mark the segment to
     * auto-destroy when the number of attached processes becomes 0.
     *
     * IMPORTANT: There is a small window of vulnerability between shmget and shmctl that
     * can lead to major issues: between these calls, we have a segment of persistent
     * memory that will survive the program if it dies (e.g. someone just happens to send us
     * a SIGKILL)
     */

    assert(GM == nullptr);
    assert(gm_shmid == 0);

    // Check if POSIX shared memory is forced (for sandboxed environments where
    // harness uses glibc but children use musl/Pin CRT which may block SysV shm)
    const char* force_posix = getenv("ZSIM_FORCE_POSIX_SHM");
    if (force_posix && (strcmp(force_posix, "1") == 0 || strcmp(force_posix, "true") == 0)) {
        info("ZSIM_FORCE_POSIX_SHM is set, using POSIX shared memory");
        if (gm_try_posix_create(segmentSize)) {
            char* alloc_start = reinterpret_cast<char*>(GM) + 1024;
            size_t alloc_size = segmentSize - 1 - 1024;
            GM->base_regp = nullptr;
            GM->mspace_ptr = create_mspace_with_base(alloc_start, alloc_size, 1 /*locked*/);
            futex_init(&GM->lock);
            assert(GM->mspace_ptr);
            setenv("ZSIM_POSIX_SHM", gm_shm_name, 1);
            return -1;
        }
        panic("ZSIM_FORCE_POSIX_SHM set but POSIX shared memory creation failed");
    }

    // First try SysV shared memory
    gm_shmid = shmget(IPC_PRIVATE, segmentSize, 0644 | IPC_CREAT /*| SHM_HUGETLB*/);
    if (gm_shmid == -1) {
        int shmget_errno = errno;
        warn("SysV shmget failed (errno=%d), trying POSIX shared memory...", shmget_errno);

        // Try POSIX shared memory as fallback
        if (gm_try_posix_create(segmentSize)) {
            // Initialize the segment
            char* alloc_start = reinterpret_cast<char*>(GM) + 1024;
            size_t alloc_size = segmentSize - 1 - 1024;
            GM->base_regp = nullptr;
            GM->mspace_ptr = create_mspace_with_base(alloc_start, alloc_size, 1 /*locked*/);
            futex_init(&GM->lock);
            assert(GM->mspace_ptr);

            // Return a fake shmid that encodes POSIX mode
            // We'll use the environment to pass the shm name to children
            setenv("ZSIM_POSIX_SHM", gm_shm_name, 1);
            return -1;  // Special value indicating POSIX mode
        }

        panic("Both SysV and POSIX shared memory failed. Cannot continue.");
    }

    GM = static_cast<gm_segment*>(syscall_shmat(gm_shmid, GM_BASE_ADDR, 0));
    if (GM != GM_BASE_ADDR) {
        int shmat_errno = errno;

        // Clean up SysV attempt
        if (GM != (void*)-1) {
            syscall_shmdt(GM);
        }
        shmctl(gm_shmid, IPC_RMID, nullptr);
        gm_shmid = 0;
        GM = nullptr;

        warn("SysV shmat failed (errno=%d), trying POSIX shared memory...", shmat_errno);

        // Try POSIX shared memory as fallback
        if (gm_try_posix_create(segmentSize)) {
            // Initialize the segment
            char* alloc_start = reinterpret_cast<char*>(GM) + 1024;
            size_t alloc_size = segmentSize - 1 - 1024;
            GM->base_regp = nullptr;
            GM->mspace_ptr = create_mspace_with_base(alloc_start, alloc_size, 1 /*locked*/);
            futex_init(&GM->lock);
            assert(GM->mspace_ptr);

            setenv("ZSIM_POSIX_SHM", gm_shm_name, 1);
            return -1;  // Special value indicating POSIX mode
        }

        panic("Both SysV and POSIX shared memory failed. Cannot continue.");
    }

    //Mark the segment to auto-destroy when the number of attached processes becomes 0.
    int ret = shmctl(gm_shmid, IPC_RMID, nullptr);
    assert(!ret);

    char* alloc_start = reinterpret_cast<char*>(GM) + 1024;
    size_t alloc_size = segmentSize - 1 - 1024;
    GM->base_regp = nullptr;

    GM->mspace_ptr = create_mspace_with_base(alloc_start, alloc_size, 1 /*locked*/);
    futex_init(&GM->lock);
    assert(GM->mspace_ptr);

    return gm_shmid;
}

void gm_attach(int shmid) {
    assert(GM == nullptr);
    assert(gm_shmid == 0);

    // Check if we should use POSIX shared memory
    const char* posix_shm = getenv("ZSIM_POSIX_SHM");
    if (posix_shm && posix_shm[0] != '\0') {
        info("Using POSIX shared memory: %s", posix_shm);
        if (gm_try_posix_attach(posix_shm)) {
            return;  // Successfully attached via POSIX
        }
        warn("POSIX attach failed, trying SysV with shmid=%d", shmid);
    }

    gm_shmid = shmid;

    // First try fixed address
    GM = static_cast<gm_segment*>(syscall_shmat(gm_shmid, GM_BASE_ADDR, 0));
    if (GM == GM_BASE_ADDR) {
        gm_reloc_offset = 0;  // No relocation needed
        return;
    }

    // Fixed address failed (EPERM in Pin 4.x) - try without fixed address
    if (GM != (void*)-1) {
        syscall_shmdt(GM);  // Detach from wrong address
    }

    // Try attaching at NULL (let OS choose address)
    GM = static_cast<gm_segment*>(syscall_shmat(gm_shmid, nullptr, 0));
    if (GM == (void*)-1) {
        panic("gm_attach: shmat(shmid=%d, addr=NULL) failed, errno=%d. "
              "Cannot attach shared memory at any address.",
              shmid, errno);
    }

    // Success! Calculate the relocation offset
    gm_reloc_offset = (intptr_t)GM - (intptr_t)GM_BASE_ADDR;
    info("gm_attach: Attached at %p instead of %p (reloc_offset=%ld)",
         GM, GM_BASE_ADDR, gm_reloc_offset);

    // NOTE: This means all pointers stored in shared memory need to be
    // adjusted by gm_reloc_offset when dereferenced. This requires using
    // gm_relocate_ptr() for all pointer accesses from shared memory.
}

// Relocate a pointer stored in shared memory to the current process's address space
void* gm_relocate_ptr(void* ptr) {
    if (ptr == nullptr || gm_reloc_offset == 0) return ptr;
    return (void*)((intptr_t)ptr + gm_reloc_offset);
}

// Convert a local pointer back to the canonical form for storage in shared memory
void* gm_canonicalize_ptr(void* ptr) {
    if (ptr == nullptr || gm_reloc_offset == 0) return ptr;
    return (void*)((intptr_t)ptr - gm_reloc_offset);
}

// Check if relocation is active
bool gm_needs_relocation() {
    return gm_reloc_offset != 0;
}


void* gm_malloc(size_t size) {
    assert(GM);
    assert(GM->mspace_ptr);
    futex_lock(&GM->lock);
    void* ptr = mspace_malloc(GM->mspace_ptr, size);
    futex_unlock(&GM->lock);
    if (!ptr) panic("gm_malloc(): Out of global heap memory, use a larger GM segment");
    return ptr;
}

void* __gm_calloc(size_t num, size_t size) {
    assert(GM);
    assert(GM->mspace_ptr);
    futex_lock(&GM->lock);
    void* ptr = mspace_calloc(GM->mspace_ptr, num, size);
    futex_unlock(&GM->lock);
    if (!ptr) panic("gm_calloc(): Out of global heap memory, use a larger GM segment");
    return ptr;
}

void* gm_memalign(size_t blocksize, size_t bytes) {
    assert(GM);
    assert(GM->mspace_ptr);
    futex_lock(&GM->lock);
    void* ptr = mspace_memalign(GM->mspace_ptr, blocksize, bytes);
    futex_unlock(&GM->lock);
    if (!ptr) panic("gm_memalign(): Out of global heap memory, use a larger GM segment");
    return ptr;
}

char* gm_strdup(const char* str) {
    if (str == nullptr) return nullptr;
    size_t len = strlen(str);
    char* ptr = static_cast<char*>(gm_malloc(len+1));
    strcpy(ptr, str);
    return ptr;
}

void gm_free(void* ptr) {
    //dsm: With dlmalloc, space is reclaimed but not returned to the OS.
    //We'll want to use some threshold or strategy to return pages to the system on free(), some time in the future
    assert(GM);
    assert(GM->mspace_ptr);
    futex_lock(&GM->lock);
    mspace_free(GM->mspace_ptr, ptr);
    futex_unlock(&GM->lock);
}

void gm_set_glob_ptr(void* ptr) {
    // Store the pointer as-is (harness uses fixed address, no relocation)
    GM->base_regp = ptr;
    // Ensure write is visible
    __sync_synchronize();
}

void* gm_get_glob_ptr() {
    // Apply relocation if needed (child processes may have different base address)
    void* ptr = const_cast<void*>(GM->base_regp);
    return gm_relocate_ptr(ptr);
}

void gm_set_secondary_ptr(void* ptr) {
    GM->secondary_regp = ptr;
}

void* gm_get_secondary_ptr() {
    void* ptr = const_cast<void*>(GM->secondary_regp);
    return gm_relocate_ptr(ptr);
}

bool gm_isready() {
    return gm_get_glob_ptr() != nullptr;
}

size_t gm_mem_usage() {
    return mspace_mallinfo(GM->mspace_ptr).uordblks;
}

int gm_get_shmid() {
    return gm_shmid;
}
