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

#include "debug_zsim.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "log.h"

// Check for libelf availability at compile time
#if __has_include(<gelf.h>)
#define HAVE_LIBELF 1
#include <gelf.h>
#include <link.h>
#else
#define HAVE_LIBELF 0
#include <link.h>
#endif

/* This file is pretty much self-contained, and has minimal external dependencies.
 * Please keep it this way, and ESPECIALLY don't include Pin headers since there
 * seem to be conflicts between those and some system headers.
 */

#if HAVE_LIBELF
static int pp_callback(dl_phdr_info* info, size_t size, void* data) {
    bool matched = strstr(info->dlpi_name, "libzsim_qemu.so") || strstr(info->dlpi_name, "libzsim.so");
    /* For standalone executables (zsim_trace), dl_iterate_phdr reports "" for
     * the main binary.  Resolve via /proc/self/exe to check the name. */
    char exe_path[1024];
    const char* path_to_open = info->dlpi_name;
    if (!matched && info->dlpi_name[0] == '\0') {
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len > 0) {
            exe_path[len] = '\0';
            path_to_open = exe_path;
            matched = (strstr(exe_path, "zsim_trace") != nullptr) ||
                      (strstr(exe_path, "pimid") != nullptr);
        }
    }
    if (matched) {
        int fd;
        Elf* e;
        Elf_Scn* scn;
        if ((fd = open (path_to_open, O_RDONLY , 0)) < 0)
            panic("Opening %s failed", path_to_open);
        elf_version(EV_CURRENT);
        if ((e = elf_begin(fd, ELF_C_READ, nullptr)) == nullptr)
            panic("elf_begin() failed");
        size_t shstrndx; //we need this to get the section names
        if (elf_getshdrstrndx(e, &shstrndx) != 0)
            panic("elf_getshdrstrndx() failed");

        LibInfo* offsets = static_cast<LibInfo*>(data);
        offsets->textAddr = nullptr;
        offsets->dataAddr = nullptr;
        offsets->bssAddr = nullptr;

        scn = nullptr;
        while ((scn = elf_nextscn(e, scn)) != nullptr) {
            GElf_Shdr shdr;
            if (gelf_getshdr(scn, &shdr) != &shdr)
                panic("gelf_getshdr() failed");
            char* name = elf_strptr(e, shstrndx , shdr.sh_name);
            //info("Section %s %lx %lx", name, shdr.sh_addr, shdr.sh_offset);
            //info("Section %s %lx %lx\n", name, info->dlpi_addr + shdr.sh_addr, info->dlpi_addr + shdr.sh_offset);
            void* sectionAddr = reinterpret_cast<void*>(info->dlpi_addr + shdr.sh_addr);
            if (strcmp(".text", name) == 0) {
                offsets->textAddr = sectionAddr;
            } else if (strcmp(".data", name) == 0) {
                offsets->dataAddr = sectionAddr;
            } else if (strcmp(".bss", name) == 0) {
                offsets->bssAddr = sectionAddr;
            }
        }
        elf_end(e);
        close(fd);

        //Check that we got all the section addresses; it'd be extremely weird if we didn't
        assert(offsets->textAddr && offsets->dataAddr && offsets->bssAddr);

        return 1; //stops iterating
    }
    return 0; //continues iterating
}

void getLibzsimAddrs(LibInfo* libzsimAddrs) {
    int ret = dl_iterate_phdr(pp_callback, libzsimAddrs);
    if (ret != 1) panic("libzsim.so / libzsim_qemu.so not found");
}

#else // !HAVE_LIBELF - provide stub implementation

// Stub callback that gets base address from program headers
static int pp_callback_stub(dl_phdr_info* info, size_t size, void* data) {
    bool matched = strstr(info->dlpi_name, "libzsim_qemu.so") || strstr(info->dlpi_name, "libzsim.so");
    if (!matched && info->dlpi_name[0] == '\0') {
        char exe_path[1024];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len > 0) { exe_path[len] = '\0'; matched = (strstr(exe_path, "zsim_trace") != nullptr) || (strstr(exe_path, "pimid") != nullptr); }
    }
    if (matched) {
        LibInfo* offsets = static_cast<LibInfo*>(data);
        // Without libelf, we can't get exact section addresses
        // Use the load address as a rough approximation
        offsets->textAddr = reinterpret_cast<void*>(info->dlpi_addr);
        offsets->dataAddr = reinterpret_cast<void*>(info->dlpi_addr);
        offsets->bssAddr = reinterpret_cast<void*>(info->dlpi_addr);
        return 1;
    }
    return 0;
}

void getLibzsimAddrs(LibInfo* libzsimAddrs) {
    warn("libelf not available - debug symbol lookup disabled");
    int ret = dl_iterate_phdr(pp_callback_stub, libzsimAddrs);
    if (ret != 1) {
        // Fallback: set addresses to null if libzsim.so not found
        libzsimAddrs->textAddr = nullptr;
        libzsimAddrs->dataAddr = nullptr;
        libzsimAddrs->bssAddr = nullptr;
    }
}

#endif // HAVE_LIBELF


void notifyHarnessForDebugger(int harnessPid) {
    kill(harnessPid, SIGUSR1);
    sleep(1); //this is a bit of a hack, but ensures the debugger catches us
}
