#include "../src/mod_hash.h"

#include <stdio.h>

int main(void)
{
    uint32_t storage[0x2002];
    size_t i;

    for (i = 0; i < sizeof(storage) / sizeof(storage[0]); ++i)
        storage[i] = UINT32_C(0xcccccccc);

    nox_mod_hash_bucket_heads_init(&storage[1], 0x2000);

    if (storage[0] != UINT32_C(0xcccccccc)
        || storage[0x2001] != UINT32_C(0xcccccccc)) {
        fprintf(stderr, "hash bucket initialization wrote beyond the table\n");
        return 1;
    }
    for (i = 1; i <= 0x2000; ++i) {
        if (storage[i] != 0) {
            fprintf(stderr, "dirty hash bucket %zu was not cleared\n", i - 1);
            return 1;
        }
    }
    return 0;
}
