#ifndef NOX_MOD_HASH_H
#define NOX_MOD_HASH_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* A newly allocated recovered hash table has no chains until inserts occur. */
static inline void nox_mod_hash_bucket_heads_init(uint32_t *heads,
                                                   size_t bucket_count)
{
  memset(heads, 0, bucket_count * sizeof(*heads));
}

#endif
