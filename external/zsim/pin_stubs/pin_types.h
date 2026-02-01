/* Stub pin_types.h - defines PIN types using stdint.h */
#ifndef PIN_TYPES_STUB_H
#define PIN_TYPES_STUB_H

/* Define __GLIBC_PREREQ and __GNUC_PREREQ macros for Pin 4.x compatibility.
 * Pin 4.x's runtime features.h doesn't define these, but system C++ headers use them.
 * We define them to return false (0) so feature checks are conservative.
 */
#ifndef __GLIBC_PREREQ
#define __GLIBC_PREREQ(maj, min) 0
#endif

#ifndef __GNUC_PREREQ
#ifdef __GNUC__
#define __GNUC_PREREQ(maj, min) \
    ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
#define __GNUC_PREREQ(maj, min) 0
#endif
#endif

/* Define __GLIBC_USE for glibc compatibility */
#ifndef __GLIBC_USE
#define __GLIBC_USE(F) 0
#endif

/* Pin 4.x pinrt compatibility: _arch_long is used by pinrt headers
 * For x86_64 Linux, it should be 'long' (64-bit).
 */
#ifndef _arch_long
#define _arch_long long
#endif

/* Pin 4.x pinrt compatibility: __locale_t for glibc interop */
#ifndef __locale_t
struct __locale_struct;
typedef struct __locale_struct *__locale_t;
#endif

/* Pin 4.x QUERY_MEM_* definitions from pinrt/include/pinos/pbits/__mman.h
 * We include them here to avoid adding pbits to include path (which has errno.h conflict)
 */
#ifndef QUERY_MEM_MAPPED
#define QUERY_MEM_FLAGS_PASSTHROUGH 0x1
#define QUERY_MEM_MAPPED 0x1
#define QUERY_MEM_R 0x2
#define QUERY_MEM_W 0x4
#define QUERY_MEM_X 0x8
#define QUERY_MEM_PROT_MASK (QUERY_MEM_R | QUERY_MEM_W | QUERY_MEM_X)
#define QUERY_MEM_SHARED 0x10
#define QUERY_MEM_BACKED 0x20
#define QUERY_MEM_WIN_IMAGE 0x40
#define QUERY_MEM_ROOT_PIN_BINARY 0x100
#define QUERY_MEM_COMMITTED 0x8000
#define QUERY_MEM_APPLICATION 0x10000
#define QUERY_MEM_STRONG 0x20000
#define QUERY_MEM_WEAK 0x40000
#define QUERY_MEM_INUSE 0x80000
#define QUERY_MEM_REMAP 0x100000
#define QUERY_MEM_FAILED_QUERY 0x80000000
#endif

/* Pin 4.x extra syscall numbers from pinrt/include/pinos/pbits/extrasyscalls.h
 * These need to be defined before pinrt_adaptor headers use them
 */
#ifndef SYS_native_syscall_unsafe
#define SYS_native_syscall_unsafe 900
#define SYS_native_syscall_safe 901
#define SYS_mman_query_range 982
#endif

/* Include string.h for memset - Pin 4.x's types_vmapi.PH uses it
 * Use C header for musl-gcc compatibility
 */
#include <string.h>

/* PIN_DISABLE_CRT_REG_DEF prevents Pin from defining REG_* macros that
 * conflict with musl's signal.h definitions. This should be defined
 * before including any Pin headers.
 */
#ifndef PIN_DISABLE_CRT_REG_DEF
#define PIN_DISABLE_CRT_REG_DEF
#endif

#include <stdint.h>

/* For C code, include stdbool.h for 'bool' type */
#ifndef __cplusplus
#include <stdbool.h>
#endif

/* Basic PIN types */
typedef uint8_t  UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef int8_t   INT8;
typedef int16_t  INT16;
typedef int32_t  INT32;
typedef int64_t  INT64;

/* Pointer-sized types for 64-bit */
typedef uint64_t ADDRINT;
typedef int64_t  ADDRDELTA;
typedef uint64_t USIZE;
typedef int64_t  SSIZE;

/* Boolean - must be bool to match Pin's typedef */
typedef bool BOOL;
/* TRUE/FALSE as integers so they can be used as null pointers when needed */
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* Void pointer */
typedef void VOID;

#endif /* PIN_TYPES_STUB_H */
