#ifndef NOX_LEGACY_TABLE_H
#define NOX_LEGACY_TABLE_H

#include <stddef.h>
#include <stdint.h>

/* Search a sentinel-terminated table of recovered DWORD records. All offsets
 * describe the original fixed-width records; host pointer size is irrelevant. */
static inline uint32_t nox_legacy_table_value_by_key32(
    const void *records, uint32_t key, size_t stride, size_t key_offset,
    size_t value_offset, size_t next_record_marker_offset)
{
  const unsigned char *record = (const unsigned char *)records;

  for (;; record += stride)
  {
    if ( *(const uint32_t *)(record + key_offset) == key )
      return *(const uint32_t *)(record + value_offset);
    if ( !*(const uint32_t *)(record + next_record_marker_offset) )
      return 0;
  }
}

#endif
