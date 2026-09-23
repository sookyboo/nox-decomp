#include "../src/native_pointer.h"

#include <stdio.h>

static int fixed_image_pointer_decode_test(void)
{
    const uintptr_t image_address = UINT64_C(0x0000001200000000);
    const uintptr_t data_begin = image_address + UINT32_C(0x1b000000);
    const uintptr_t data_end = image_address + UINT32_C(0x1c000000);
    const uint32_t encoded = UINT32_C(0x1b508f94);
    const uintptr_t expected = image_address + encoded;

    return nox_native_fixed_pointer_decode32(encoded, image_address,
                                             data_begin, data_end) == expected;
}

static int low_word_static_pointer_decode_test(void)
{
    const uintptr_t image_address = UINT64_C(0x0000001200000000);
    const uintptr_t data_begin = image_address + UINT32_C(0x01800000);
    const uintptr_t data_end = image_address + UINT32_C(0x01840000);
    const uint32_t encoded = UINT32_C(0x0183bc5c);

    return nox_native_fixed_pointer_decode32(encoded, image_address,
                                             data_begin, data_end)
        == image_address + encoded;
}

static int second_static_data_range_decode_test(void)
{
    const uintptr_t image_address = UINT64_C(0x0000001200000000);
    const uintptr_t data_begin = image_address + UINT32_C(0x26000000);
    const uintptr_t data_end = image_address + UINT32_C(0x26500000);
    const uint32_t encoded = UINT32_C(0x2620c770);

    return nox_native_fixed_pointer_decode32(encoded, image_address,
                                             data_begin, data_end)
        == image_address + encoded;
}

static int low_word_audio_state_pointer_decode_test(void)
{
    const uintptr_t image_address = UINT64_C(0x0000001200000000);
    const uintptr_t data_begin = image_address + UINT32_C(0x19000000);
    const uintptr_t data_end = image_address + UINT32_C(0x1a000000);
    const uint32_t encoded = UINT32_C(0x1932fdec);

    return nox_native_fixed_pointer_decode32(encoded, image_address,
                                             data_begin, data_end)
        == image_address + encoded;
}

static int low_address_static_link_decode_test(void)
{
    const uintptr_t image_address = UINT64_C(0x0000001200000000);
    const uintptr_t data_begin = image_address + UINT32_C(0x4e000000);
    const uintptr_t data_end = image_address + UINT32_C(0x4f000000);
    const uint32_t encoded = UINT32_C(0x4e83fefc);

    return nox_native_fixed_pointer_decode32(encoded, image_address,
                                             data_begin, data_end)
        == image_address + encoded;
}

static int legacy_pointer_slot_reads_only_one_dword_test(void)
{
    const uint32_t slots[2] = {UINT32_C(0x43645738), UINT32_C(0x00200020)};

    return nox_native_pointer_slot32_read(&slots[0]) == slots[0];
}

static int code_pointer_decode_test(void)
{
    const uintptr_t image_address = UINT64_C(0x0000001200000000);
    const uint32_t encoded = UINT32_C(0x4581a2be);
    const uintptr_t expected = image_address + encoded;

    return nox_native_image_pointer_decode32(encoded, image_address) == expected;
}

static int non_image_pointer_stays_low_test(void)
{
    const uintptr_t image_address = UINT64_C(0x0000001200000000);
    const uintptr_t data_begin = image_address + UINT32_C(0x1b000000);
    const uintptr_t data_end = image_address + UINT32_C(0x1c000000);
    const uint32_t encoded = UINT32_C(0x1e000000);

    return nox_native_fixed_pointer_decode32(encoded, image_address,
                                             data_begin, data_end) == encoded;
}

static int image_data_range_test(void)
{
    const uintptr_t image_address = UINT64_C(0x0000001200000000);
    const uintptr_t first_data_begin = image_address + UINT32_C(0x1f000000);
    const uintptr_t first_data_end = image_address + UINT32_C(0x1f400000);
    const uintptr_t second_data_begin = image_address + UINT32_C(0x24a00000);
    const uintptr_t second_data_end = image_address + UINT32_C(0x28000000);

    return nox_native_image_pointer_in_range32(UINT32_C(0x1f123456),
                                               image_address,
                                               first_data_begin,
                                               first_data_end)
        && nox_native_image_pointer_in_range32(UINT32_C(0x24b12345),
                                               image_address,
                                               second_data_begin,
                                               second_data_end)
        && !nox_native_image_pointer_in_range32(UINT32_C(0x1e123456),
                                                image_address,
                                                first_data_begin,
                                                first_data_end);
}

static int low_word_executable_static_data_decode_test(void)
{
    /* Mirrors the amd64 core's low-word encoding: the image is above 4 GiB,
     * but its static string pointer begins with 0x14836530, below the legacy
     * low-allocation cutoff. */
    const uintptr_t image_address = UINT64_C(0x000055b01480db60);
    const uintptr_t first_begin = UINT64_C(0x000055b014836000);
    const uintptr_t first_end = UINT64_C(0x000055b014860000);
    const uintptr_t second_begin = UINT64_C(0x000055b014900000);
    const uintptr_t second_end = UINT64_C(0x000055b014a00000);
    const uint32_t encoded = UINT32_C(0x14836530);

    return nox_native_static_data_pointer_decode32(
               encoded, image_address, first_begin, first_end,
               second_begin, second_end)
        == UINT64_C(0x000055b014836530);
}

static int window_field_pointer_decode_test(void)
{
    const uintptr_t image_address = UINT64_C(0x000055b41400db60);
    const uint32_t low_word = UINT32_C(0x173b0cc0);

    return nox_native_window_field_pointer_decode32(
               low_word, image_address, 0)
               == UINT64_C(0x000055b4173b0cc0)
        && nox_native_window_field_pointer_decode32(
               low_word, image_address, 1)
               == (uintptr_t)low_word;
}

static int window_text_value_retains_host_pointer_test(void)
{
    uintptr_t values[3];
    const uintptr_t text = UINT64_C(0x5561feaceb60);

    nox_native_window_text_values_init(values, text, 1, 0);
    if (values[0] != text || values[1] != 1 || values[2] != 0)
        return 0;
    nox_native_window_text_value_set(values, UINT64_C(0x7fff123456789abc));
    return values[0] == UINT64_C(0x7fff123456789abc);
}

int main(void)
{
    if (!fixed_image_pointer_decode_test()) {
        fprintf(stderr, "failed to reconstruct a low-word game-image pointer\n");
        return 1;
    }
    if (!low_word_static_pointer_decode_test()) {
        fprintf(stderr, "failed to distinguish low-word static data from a low allocation\n");
        return 1;
    }
    if (!second_static_data_range_decode_test()) {
        fprintf(stderr, "failed to reconstruct static data from the second image array\n");
        return 1;
    }
    if (!low_word_audio_state_pointer_decode_test()) {
        fprintf(stderr, "failed to reconstruct a low-word audio-state pointer\n");
        return 1;
    }
    if (!low_address_static_link_decode_test()) {
        fprintf(stderr, "failed to reconstruct a low-address static list link\n");
        return 1;
    }
    if (!legacy_pointer_slot_reads_only_one_dword_test()) {
        fprintf(stderr, "legacy pointer slot read consumed its adjacent DWORD\n");
        return 1;
    }
    if (!code_pointer_decode_test()) {
        fprintf(stderr, "failed to reconstruct a low-word game-image callback\n");
        return 1;
    }
    if (!non_image_pointer_stays_low_test()) {
        fprintf(stderr, "non-image legacy pointer was incorrectly widened\n");
        return 1;
    }
    if (!image_data_range_test()) {
        fprintf(stderr, "static image data pointer range was not recognized\n");
        return 1;
    }
    if (!low_word_executable_static_data_decode_test()) {
        fprintf(stderr, "low-word executable static data was mistaken for a low allocation\n");
        return 1;
    }
    if (!window_field_pointer_decode_test()) {
        fprintf(stderr, "window data pointer did not preserve low/high address provenance\n");
        return 1;
    }
    if (!window_text_value_retains_host_pointer_test()) {
        fprintf(stderr, "localized window text pointer was truncated in its native values\n");
        return 1;
    }
    if (nox_native_fixed_pointer_decode32(0, UINT64_C(0x1200000000), 0, 0) != 0) {
        fprintf(stderr, "null legacy pointer was not preserved\n");
        return 1;
    }
    return 0;
}
