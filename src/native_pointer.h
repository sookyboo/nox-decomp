#ifndef NOX_NATIVE_POINTER_H
#define NOX_NATIVE_POINTER_H

#include <stdint.h>

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
