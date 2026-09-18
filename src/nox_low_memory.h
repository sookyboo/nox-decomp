#ifndef NOX_LOW_MEMORY_H
#define NOX_LOW_MEMORY_H

#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

/* Recovered records retain 32-bit pointer slots even in native 64-bit builds.
 * Keep allocations used by those records below the 4 GiB boundary. */
static inline void *nox_low_alloc(size_t size)
{
    if (!size)
        return 0;

#if defined(_WIN32)
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    size_t page_size = (size_t)system_info.dwPageSize;
    size_t mapped_size = (size + page_size - 1) & ~(page_size - 1);
    static uintptr_t next_hint = 0x10000000u;

    /* Several recovered consumers cast the legacy slot through signed int
     * before widening it again. Keep allocations below 2 GiB, not merely
     * below the 4 GiB DWORD boundary, so those casts cannot sign-extend. */
    for (uintptr_t hint = next_hint; hint < 0x70000000u; hint += page_size) {
        void *result = VirtualAlloc((void *)hint, mapped_size,
                                     MEM_RESERVE | MEM_COMMIT,
                                     PAGE_READWRITE);
        if (!result)
            continue;
        if ((uintptr_t)result == hint
            && (uintptr_t)result <= (uintptr_t)INT32_MAX - size)
        {
            next_hint = hint + mapped_size;
            return result;
        }
        VirtualFree(result, 0, MEM_RELEASE);
    }
    return 0;
#elif defined(__linux__)
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    if (!page_size)
        return 0;
    size_t mapped_size = (size + page_size - 1) & ~(page_size - 1);
    void *result = mmap(0, mapped_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    return result == MAP_FAILED ? 0 : result;
#else
    return malloc(size);
#endif
}

static inline void nox_low_free(void *address, size_t size)
{
    if (!address || !size)
        return;

#if defined(_WIN32)
    (void)size;
    VirtualFree(address, 0, MEM_RELEASE);
#elif defined(__linux__)
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    if (!page_size)
        return;
    size_t mapped_size = (size + page_size - 1) & ~(page_size - 1);
    munmap(address, mapped_size);
#else
    (void)size;
    free(address);
#endif
}

#endif
