#ifndef NOX_TEST_LEGACY_MEMORY_H
#define NOX_TEST_LEGACY_MEMORY_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

/* Focused fixtures model records from the original 32-bit game. Their
 * pointer slots remain four bytes wide in the recovered record layout. */
static inline void *nox_test_legacy_alloc_aligned(size_t size, size_t alignment)
{
    if (!size || alignment == 0 || (alignment & (alignment - 1)) != 0)
        return NULL;

#if defined(_WIN32)
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    size_t page_size = (size_t)system_info.dwPageSize;
    size_t mapped_size = (size + alignment - 1) & ~(alignment - 1);
    mapped_size = (mapped_size + page_size - 1) & ~(page_size - 1);
    for (uintptr_t hint = 0x10000000u; hint < 0xf0000000u; hint += 0x01000000u) {
        void *mapping = VirtualAlloc((void *)hint, mapped_size,
                                     MEM_RESERVE | MEM_COMMIT,
                                     PAGE_READWRITE);
        if (!mapping)
            continue;
        uintptr_t address = ((uintptr_t)mapping + alignment - 1) & ~(alignment - 1);
        if (address <= UINT32_MAX - size) {
            void *memory = (void *)address;
            memset(memory, 0, size);
            return memory;
        }
        VirtualFree(mapping, 0, MEM_RELEASE);
    }
    return NULL;
#elif defined(__x86_64__)
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
        return NULL;
    size_t page_mask = (size_t)page_size - 1;
    size_t mapped_size = (size + alignment - 1 + page_mask) & ~page_mask;
    void *mapping = mmap(NULL, mapped_size + alignment - 1,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (mapping == MAP_FAILED)
        return NULL;
    uintptr_t address = ((uintptr_t)mapping + alignment - 1) & ~(alignment - 1);
    if (address > UINT32_MAX || size > UINT32_MAX - address)
        return NULL;
    void *memory = (void *)address;
    memset(memory, 0, size);
    return memory;
#else
    void *memory = NULL;
    if (posix_memalign(&memory, alignment, size) != 0)
        return NULL;
    memset(memory, 0, size);
    return memory;
#endif
}

static inline void *nox_test_legacy_alloc(size_t size)
{
    return nox_test_legacy_alloc_aligned(size, 4096);
}

#endif
