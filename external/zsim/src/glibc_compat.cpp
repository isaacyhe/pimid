/**
 * glibc_compat.cpp - Compatibility stubs for Pin 4.x musl-based loader
 *
 * Pin 4.x uses a musl-based loader (libpincrt.so) that provides its own
 * libc. However, system shared libraries loaded transitively (e.g., libelf,
 * libzstd) may reference glibc-specific symbols not present in musl:
 *   - __memcpy_chk, __memmove_chk, etc. (from -D_FORTIFY_SOURCE)
 *   - _dl_find_object (glibc 2.35+ stack unwinding optimization)
 *
 * These weak stubs ensure the pintool loads successfully. The actual
 * functionality is either provided by Pin's runtime or by the standard
 * C functions (without buffer overflow checking).
 */

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

extern "C" {

/* Fortified string/memory functions - fall back to unchecked versions */

__attribute__((weak, visibility("default")))
void* __memcpy_chk(void* dest, const void* src, size_t n, size_t destlen) {
    (void)destlen;
    return memcpy(dest, src, n);
}

__attribute__((weak, visibility("default")))
void* __memmove_chk(void* dest, const void* src, size_t n, size_t destlen) {
    (void)destlen;
    return memmove(dest, src, n);
}

__attribute__((weak, visibility("default")))
void* __memset_chk(void* s, int c, size_t n, size_t slen) {
    (void)slen;
    return memset(s, c, n);
}

__attribute__((weak, visibility("default")))
char* __strcpy_chk(char* dest, const char* src, size_t destlen) {
    (void)destlen;
    return strcpy(dest, src);
}

__attribute__((weak, visibility("default")))
char* __strncpy_chk(char* dest, const char* src, size_t n, size_t destlen) {
    (void)destlen;
    return strncpy(dest, src, n);
}

__attribute__((weak, visibility("default")))
char* __strcat_chk(char* dest, const char* src, size_t destlen) {
    (void)destlen;
    return strcat(dest, src);
}

__attribute__((weak, visibility("default")))
char* __strncat_chk(char* dest, const char* src, size_t n, size_t destlen) {
    (void)destlen;
    return strncat(dest, src, n);
}

__attribute__((weak, visibility("default")))
int __sprintf_chk(char* s, int flag, size_t slen, const char* format, ...) {
    (void)flag;
    (void)slen;
    va_list ap;
    va_start(ap, format);
    int ret = vsprintf(s, format, ap);
    va_end(ap);
    return ret;
}

__attribute__((weak, visibility("default")))
int __snprintf_chk(char* s, size_t maxlen, int flag, size_t slen, const char* format, ...) {
    (void)flag;
    (void)slen;
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(s, maxlen, format, ap);
    va_end(ap);
    return ret;
}

__attribute__((weak, visibility("default")))
int __fprintf_chk(FILE* stream, int flag, const char* format, ...) {
    (void)flag;
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

__attribute__((weak, visibility("default")))
int __printf_chk(int flag, const char* format, ...) {
    (void)flag;
    va_list ap;
    va_start(ap, format);
    int ret = vprintf(format, ap);
    va_end(ap);
    return ret;
}

/* glibc 2.35+ stack unwinding optimization - not available in musl.
 * Use asm name to avoid conflicting with dlfcn.h declaration on glibc 2.35+. */
__attribute__((weak, visibility("default")))
int __pimid_dl_find_object_stub(void* pc, void* result) __asm__("_dl_find_object");

__attribute__((weak, visibility("default")))
int __pimid_dl_find_object_stub(void* pc, void* result) {
    (void)pc;
    (void)result;
    return -1;  /* Signal failure, unwinder falls back to dl_iterate_phdr */
}

/* Stack protection */
__attribute__((weak, visibility("default")))
void __stack_chk_fail(void) {
    abort();
}

__attribute__((weak, visibility("default")))
void __stack_chk_fail_local(void) {
    abort();
}

} /* extern "C" */
