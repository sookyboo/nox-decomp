#ifndef NOX_NATIVE_POINTER_H
#define NOX_NATIVE_POINTER_H

#include <stdint.h>

/* Recovered pointer slots remain 32-bit even when the host pointer is wider.
 * Read only the encoded word; the next DWORD may be unrelated game state. */
static inline uint32_t nox_native_pointer_slot32_read(const void *slot)
{
  return *(const uint32_t *)slot;
}

/* Rebuild a recovered DWORD pointer into the native game-image mapping. */
static inline uintptr_t nox_native_image_pointer_decode32(uint32_t value,
                                                           uintptr_t image_address)
{
  if ( !value )
    return 0;
  return (image_address & ~(uintptr_t)UINT32_MAX) | (uintptr_t)value;
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
  if ( value < 0x50000000u )
    return value;
  return candidate;
}

/* Window-specific data pointers may refer either to a real low mapping or to
 * a host allocation whose recovered DWORD contains only its low word. */
static inline uintptr_t nox_native_window_field_pointer_decode32(
    uint32_t value, uintptr_t image_address, int low_address_is_mapped)
{
  if ( !value )
    return 0;
  if ( value < 0x50000000u && low_address_is_mapped )
    return value;
  return nox_native_image_pointer_decode32(value, image_address);
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
