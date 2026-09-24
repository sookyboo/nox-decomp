#include "../src/native_pointer.h"
#include "../src/legacy_table.h"

#include <stdio.h>
#include <string.h>
#if UINTPTR_MAX > UINT32_MAX && defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

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

static int legacy_table_key_lookup_uses_dword_records_test(void)
{
    const uint32_t twelve_byte_records[] = {
        UINT32_C(0x1000), UINT32_C(0x1111), UINT32_C(0x47d),
        UINT32_C(0x2000), UINT32_C(0x2222), UINT32_C(0x400),
        0, 0, 0,
    };
    const uint32_t twenty_four_byte_records[] = {
        UINT32_C(0x1000), UINT32_C(0x1100), UINT32_C(0x47e),
        UINT32_C(0x8000), UINT32_C(0x1200), UINT32_C(0x47d),
        UINT32_C(0x2000), UINT32_C(0x2100), UINT32_C(0x1234),
        UINT32_C(0x400), UINT32_C(0x2200), UINT32_C(0x5678),
        0, 0, 0, 0, 0, 0,
    };

    return nox_legacy_table_value_by_key32(twelve_byte_records,
               UINT32_C(0x400), 12, 8, 4, 12) == UINT32_C(0x2222)
        && nox_legacy_table_value_by_key32(twelve_byte_records,
               UINT32_C(0xffff), 12, 8, 4, 12) == 0
        && nox_legacy_table_value_by_key32(twenty_four_byte_records,
               UINT32_C(0x400), 24, 12, 8, 24) == UINT32_C(0x1234)
        && nox_legacy_table_value_by_key32(twenty_four_byte_records,
               UINT32_C(0xffff), 24, 12, 8, 24) == 0;
}

static int file_reader_cursor_uses_dword_field_test(void)
{
#if UINTPTR_MAX > UINT32_MAX && defined(__linux__) && defined(MAP_32BIT)
    const long page_size = sysconf(_SC_PAGESIZE);
    uint32_t *reader;
    unsigned char *cursor;
    int passed;

    if (page_size <= 0)
        return 0;
    reader = mmap(NULL, (size_t)page_size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);
    if (reader == MAP_FAILED)
        return 0;
    cursor = (unsigned char *)reader + 32;
    reader[2] = (uint32_t)(uintptr_t)cursor;
    reader[3] = UINT32_C(0x00200020);
    passed = nox_native_file_reader_cursor32(reader) == cursor;
    munmap(reader, (size_t)page_size);
    return passed;
#else
    uint32_t reader[4] = {0};
    unsigned char data[8] = {0};

    reader[2] = (uint32_t)(uintptr_t)data;
    return nox_native_file_reader_cursor32(reader) == data;
#endif
}

static int legacy_pointer_slot_write_preserves_neighbor_test(void)
{
    struct {
        uint32_t names[6];
        char user_color[12];
    } image = {{0}, "UserColor1"};

    nox_native_pointer_slot32_write(&image.names[5],
                                    (uintptr_t)image.user_color);
    return image.names[5] == (uint32_t)(uintptr_t)image.user_color
        && strcmp(image.user_color, "UserColor1") == 0;
}

static int polygon_name_pointer_preserves_next_vertex_count_test(void)
{
    uint32_t descriptors[8] = {0};
    const uintptr_t name = (uintptr_t)UINT64_C(0x12345678abcdef01);

    /* Each recovered polygon descriptor is 16 bytes; its name pointer at
     * +12 is adjacent to the next descriptor's vertex count. */
    descriptors[4] = 5;
    nox_native_pointer_slot32_write(&descriptors[3], name);
    return descriptors[3] == (uint32_t)name && descriptors[4] == 5;
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

static int class_selection_state_pointer_decode_test(void)
{
    const uintptr_t image_address = UINT64_C(0x00005555558c9b60);
    const uintptr_t data_begin = UINT64_C(0x0000555555800000);
    const uintptr_t data_end = UINT64_C(0x0000555555b00000);
    const uintptr_t state_begin = UINT64_C(0x0000555555900000);
    const uintptr_t state_end = UINT64_C(0x0000555556000000);
    const uint32_t encoded = UINT32_C(0x55a39da4);

    return nox_native_static_data_pointer_decode32(
               encoded, image_address, data_begin, data_end,
               state_begin, state_end)
        == UINT64_C(0x0000555555a39da4);
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

static int map32_upper_range_pointer_decode_test(void)
{
    const uintptr_t image_address = UINT64_C(0x00005555558c9b60);
    const uint32_t encoded = UINT32_C(0x72445c74);
    const uintptr_t reconstructed = UINT64_C(0x0000555572445c74);

    return nox_native_pointer_decode32(encoded, image_address, 0)
               == reconstructed
        && nox_native_pointer_decode32(encoded, image_address, 1)
               == (uintptr_t)encoded
        && nox_native_window_field_pointer_decode32(
               encoded, image_address, 1) == (uintptr_t)encoded;
}

static int indexed_legacy_record_pointer_test(void)
{
    const uintptr_t native_base = UINT64_C(0x0000555555a39da4);
    const uint32_t selected_index = 3;

    return nox_native_legacy_indexed_record(native_base, selected_index, 40)
        == native_base + 120;
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
    if (!legacy_table_key_lookup_uses_dword_records_test()) {
        fprintf(stderr, "legacy table lookup used host pointer width for DWORD records\n");
        return 1;
    }
    if (!file_reader_cursor_uses_dword_field_test()) {
        fprintf(stderr, "file-reader cursor overlapped the adjacent end field\n");
        return 1;
    }
    if (!legacy_pointer_slot_write_preserves_neighbor_test()) {
        fprintf(stderr, "legacy pointer slot write overwrote adjacent UserColor1 text\n");
        return 1;
    }
    if (!polygon_name_pointer_preserves_next_vertex_count_test()) {
        fprintf(stderr, "polygon name pointer overwrote the next vertex count\n");
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
    if (!class_selection_state_pointer_decode_test()) {
        fprintf(stderr, "class-selection data pointer was not rebuilt from its legacy slot\n");
        return 1;
    }
    if (!window_field_pointer_decode_test()) {
        fprintf(stderr, "window data pointer did not preserve low/high address provenance\n");
        return 1;
    }
    if (!map32_upper_range_pointer_decode_test()) {
        fprintf(stderr, "valid upper-range MAP_32BIT pointer was widened\n");
        return 1;
    }
    if (!indexed_legacy_record_pointer_test()) {
        fprintf(stderr, "indexed legacy record was not derived without a pointer slot\n");
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
