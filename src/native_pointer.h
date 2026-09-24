#ifndef NOX_NATIVE_POINTER_H
#define NOX_NATIVE_POINTER_H

#include <stdint.h>
#if UINTPTR_MAX > UINT32_MAX && defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

/* Linux MAP_32BIT returns mappings anywhere below 2 GiB, not just in the
 * first 1.25 GiB. Keep its complete address range when decoding DWORD slots. */
#define NOX_NATIVE_LOW_POINTER_LIMIT UINT32_C(0x80000000)

static inline int nox_native_low_address_is_mapped(uint32_t address)
{
#if UINTPTR_MAX > UINT32_MAX && defined(__linux__)
  long page_size = sysconf(_SC_PAGESIZE);
  unsigned char residency;
  uintptr_t page;

  if ( !address || page_size <= 0 )
    return 0;
  page = (uintptr_t)address - (uintptr_t)address % (uintptr_t)page_size;
  return mincore((void *)page, (size_t)page_size, &residency) == 0;
#else
  (void)address;
  return 0;
#endif
}

/* Recovered pointer slots remain 32-bit even when the host pointer is wider.
 * Read only the encoded word; the next DWORD may be unrelated game state. */
static inline uint32_t nox_native_pointer_slot32_read(const void *slot)
{
  return *(const uint32_t *)slot;
}

/* File-reader records are four DWORDs: buffer, length, cursor, and end.
 * Decode the cursor from +8 as a 32-bit pointer, not a native-width pointer
 * that would overlap the adjacent end field on 64-bit builds. */
static inline unsigned char *nox_native_file_reader_cursor32(
    const void *reader)
{
  return (unsigned char *)(uintptr_t)nox_native_pointer_slot32_read(
      (const unsigned char *)reader + 8);
}

/* Preserve the recovered four-byte slot instead of overlapping its neighbor
 * with a native-width pointer store. */
static inline void nox_native_pointer_slot32_write(void *slot,
                                                    uintptr_t value)
{
  *(uint32_t *)slot = (uint32_t)value;
}

/* Rebuild a recovered DWORD pointer into the native game-image mapping. */
static inline uintptr_t nox_native_image_pointer_decode32(uint32_t value,
                                                           uintptr_t image_address)
{
  if ( !value )
    return 0;
  return (image_address & ~(uintptr_t)UINT32_MAX) | (uintptr_t)value;
}

/* DWORDs in the upper half of the MAP_32BIT range are ambiguous with low
 * words from the game image. Preserve them only when that low page is mapped. */
static inline uintptr_t nox_native_pointer_decode32(uint32_t value,
                                                     uintptr_t image_address,
                                                     int low_address_is_mapped)
{
  if ( !value )
    return 0;
  if ( value < UINT32_C(0x50000000)
      || (value < NOX_NATIVE_LOW_POINTER_LIMIT && low_address_is_mapped) )
    return (uintptr_t)value;
  return nox_native_image_pointer_decode32(value, image_address);
}

/* Rebuild a pointer into the fixed-address data image, leaving genuine
 * low-address allocations unchanged. */
static inline uintptr_t nox_native_fixed_pointer_decode32(uint32_t value,
                                                           uintptr_t image_address,
                                                           uintptr_t data_begin,
                                                           uintptr_t data_end)
{
  uintptr_t candidate;

  if ( !value )
    return 0;
  candidate = nox_native_image_pointer_decode32(value, image_address);
  if ( candidate >= data_begin && candidate < data_end )
    return candidate;
  return (uintptr_t)value;
}

static inline int nox_native_image_pointer_in_range32(uint32_t value,
                                                       uintptr_t image_address,
                                                       uintptr_t data_begin,
                                                       uintptr_t data_end)
{
  uintptr_t candidate = nox_native_image_pointer_decode32(value, image_address);
  return candidate >= data_begin && candidate < data_end;
}

/* Static image pointers can have a low 32-bit word when ASLR places the
 * executable in the low address range. Check both game data arrays before
 * treating a low word as a genuine low allocation. */
static inline uintptr_t nox_native_static_data_pointer_decode32(
    uint32_t value, uintptr_t image_address,
    uintptr_t first_begin, uintptr_t first_end,
    uintptr_t second_begin, uintptr_t second_end)
{
  uintptr_t candidate;

  if ( !value )
    return 0;
  candidate = nox_native_image_pointer_decode32(value, image_address);
  if ( (candidate >= first_begin && candidate < first_end)
      || (candidate >= second_begin && candidate < second_end) )
    return candidate;
  return nox_native_pointer_decode32(value, image_address,
      value >= UINT32_C(0x50000000)
          && value < NOX_NATIVE_LOW_POINTER_LIMIT
          && nox_native_low_address_is_mapped(value));
}

/* Window-specific data pointers may refer either to a real low mapping or to
 * a host allocation whose recovered DWORD contains only its low word. */
static inline uintptr_t nox_native_window_field_pointer_decode32(
    uint32_t value, uintptr_t image_address, int low_address_is_mapped)
{
  if ( !value )
    return 0;
  if ( value < NOX_NATIVE_LOW_POINTER_LIMIT && low_address_is_mapped )
    return value;
  return nox_native_image_pointer_decode32(value, image_address);
}

static inline uintptr_t nox_native_legacy_indexed_record(
    uintptr_t base, uint32_t index, uint32_t stride)
{
  return base + (uintptr_t)index * stride;
}

/* Preserve a localized static-text pointer alongside recovered DWORD flags. */
static inline void nox_native_window_text_values_init(uintptr_t values[3],
                                                       uintptr_t text,
                                                       uint32_t flag1,
                                                       uint32_t flag2)
{
  values[0] = text;
  values[1] = flag1;
  values[2] = flag2;
}

static inline void nox_native_window_text_value_set(uintptr_t values[3],
                                                     uintptr_t text)
{
  values[0] = text;
}

#endif
