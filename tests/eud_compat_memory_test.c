#include <stdint.h>
#include <string.h>

#include "../src/eud_compat.h"

const char *progname = "eud_compat_memory_test";

#define NOX_EUD_DATA_BEGIN 0x00581450u
#define NOX_EUD_DATA_SPLIT_1 0x00587000u
#define NOX_EUD_DATA_SPLIT_2 0x005D4594u
#define NOX_EUD_DATA_END_EXCLUSIVE 0x0097EE69u
#define NOX_EUD_RECOVERY_DESTRUCTOR_SLOT 0x0059821Cu
#define NOX_EUD_RECOVERY_DESTRUCTOR_ORIGINAL 0x0042A6E0u
#define NOX_EUD_RECOVERY_NORMAL_GUARD 0x00852980u
#define NOX_EUD_SPELLDB_NAME_FIELD (0x00663EF0u + 80u)
#define NOX_EUD_SPELLDB_DESCRIPTION_FIELD (NOX_EUD_SPELLDB_NAME_FIELD + 4u)
#define NOX_EUD_ABILITYDB_NAME_FIELD 0x00666A24u
#define NOX_EUD_ABILITYDB_DESCRIPTION_FIELD (NOX_EUD_ABILITYDB_NAME_FIELD + 4u)

extern unsigned char byte_5D4594[3844309];

static int expect_u8(uint32_t address, uint8_t expected)
{
    uint8_t value = 0;
    return nox_eud_read_u8(address, &value) && value == expected;
}

static int write_blob(uint32_t address, const unsigned char *data, uint32_t size)
{
    uint32_t i;

    for (i = 0; i < size; ++i)
    {
        if (!nox_eud_write_u8(address + i, data[i]))
            return 0;
    }
    return 1;
}

static uint32_t raw_mapped_u32(uint32_t address)
{
    uint32_t value = 0;

    if (address < NOX_EUD_DATA_SPLIT_2 || address + 4u > NOX_EUD_DATA_END_EXCLUSIVE)
        return 0;
    memcpy(&value, byte_5D4594 + (address - NOX_EUD_DATA_SPLIT_2), sizeof(value));
    return value;
}

static uint32_t make_smart_recovery_list(uint32_t target, uint32_t original)
{
    uint32_t holder_base = nox_eud_test_alloc(12u);
    uint32_t node_base = nox_eud_test_alloc(20u);
    uint32_t holder;
    uint32_t node;

    if (!holder_base || !node_base)
        return 0;
    holder = holder_base + 8u;
    node = node_base + 8u;
    if (!nox_eud_write_u32(node, target) ||
        !nox_eud_write_u32(node + 4u, original) ||
        !nox_eud_write_u32(node + 8u, 0u) ||
        !nox_eud_write_u32(holder, node))
        return 0;
    return holder;
}

static uint32_t make_recovery_helper(int map_changed, uint32_t holder)
{
    static const unsigned char normal_template[] = {
        0x56, 0x55, 0x8B, 0x05, 0x80, 0x29, 0x85, 0x00, 0x85, 0xC0, 0x74, 0x16, 0x8B,
        0x05, 0xAC, 0xAC, 0xAC, 0xAC, 0x85, 0xC0, 0x74, 0x0C, 0x8B, 0x30, 0x8B, 0x68,
        0x04, 0x89, 0x2E, 0x8B, 0x40, 0x08, 0xEB, 0xF0, 0x5D, 0x5E, 0xC7, 0x05, 0x1C,
        0x82, 0x59, 0x00, 0xE0, 0xA6, 0x42, 0x00, 0x68, 0xE0, 0xA6, 0x42, 0x00, 0xC3};
    static const unsigned char changed_template[] = {
        0x56, 0x55, 0x8B, 0x05, 0xAC, 0xAC, 0xAC, 0xAC, 0x85, 0xC0, 0x74, 0x0C, 0x8B, 0x30,
        0x8B, 0x68, 0x04, 0x89, 0x2E, 0x8B, 0x40, 0x08, 0xEB, 0xF0, 0x5D, 0x5E, 0xC7, 0x05,
        0x1C, 0x82, 0x59, 0x00, 0xE0, 0xA6, 0x42, 0x00, 0x68, 0xE0, 0xA6, 0x42, 0x00, 0xC3,
        0x90, 0x90};
    const unsigned char *data = map_changed ? changed_template : normal_template;
    uint32_t size = map_changed ? (uint32_t)sizeof(changed_template) : (uint32_t)sizeof(normal_template);
    uint32_t base = nox_eud_test_alloc(size + 8u);
    uint32_t helper;
    uint32_t previous_offset;

    if (!base)
        return 0;
    helper = base + 8u;
    if (!write_blob(helper, data, size))
        return 0;
    if (map_changed)
    {
        if (!nox_eud_write_u32(helper + 4u, holder))
            return 0;
        previous_offset = 32u;
    }
    else
    {
        if (!nox_eud_write_u32(helper + 14u, holder))
            return 0;
        previous_offset = 42u;
    }
    if (!nox_eud_write_u32(helper + previous_offset, NOX_EUD_RECOVERY_DESTRUCTOR_ORIGINAL) ||
        !nox_eud_write_u32(helper + previous_offset + 5u, NOX_EUD_RECOVERY_DESTRUCTOR_ORIGINAL))
        return 0;
    return helper;
}

static int test_recovery_helper(int map_changed, uint32_t guard_value, int expect_restore)
{
    const uint32_t target = NOX_EUD_DATA_BEGIN + 0x180u + (uint32_t)map_changed * 4u;
    const uint32_t replacement = map_changed ? 0x2468ACE0u : 0x13579BDFu;
    uint32_t original;
    uint32_t original_guard;
    uint32_t holder;
    uint32_t helper;
    uint32_t value;

    nox_eud_reset();
    if (!nox_eud_read_u32(target, &original) ||
        !nox_eud_read_u32(NOX_EUD_RECOVERY_NORMAL_GUARD, &original_guard) ||
        !nox_eud_write_u32(target, replacement) ||
        !nox_eud_write_u32(NOX_EUD_RECOVERY_NORMAL_GUARD, guard_value))
        return 0;

    holder = make_smart_recovery_list(target, original);
    helper = make_recovery_helper(map_changed, holder);
    if (!holder || !helper ||
        !nox_eud_write_u32(NOX_EUD_RECOVERY_DESTRUCTOR_SLOT, helper) ||
        !nox_eud_read_u32(NOX_EUD_RECOVERY_DESTRUCTOR_SLOT, &value) || value != helper)
        return 0;

    nox_eud_reset();
    if (!nox_eud_read_u32(target, &value) || value != (expect_restore ? original : replacement))
        return 0;

    if (!nox_eud_write_u32(target, original) ||
        !nox_eud_write_u32(NOX_EUD_RECOVERY_NORMAL_GUARD, original_guard))
        return 0;
    return 1;
}

static int test_wide_string_field(uint32_t field_address)
{
    uint32_t original = raw_mapped_u32(field_address);
    uint32_t allocation;
    uint32_t value;
    uint32_t raw;

    nox_eud_reset();
    allocation = nox_eud_test_alloc(16u);
    if (!allocation ||
        !nox_eud_write_u16(allocation, (uint16_t)'E') ||
        !nox_eud_write_u16(allocation + 2u, (uint16_t)'U') ||
        !nox_eud_write_u16(allocation + 4u, (uint16_t)'D') ||
        !nox_eud_write_u16(allocation + 6u, 0u) ||
        !nox_eud_write_u32(field_address, allocation) ||
        !nox_eud_read_u32(field_address, &value) || value != allocation)
        return 0;

    raw = raw_mapped_u32(field_address);
    if (!raw || raw == allocation)
        return 0;

    nox_eud_reset();
    return raw_mapped_u32(field_address) == original;
}

int main(void)
{
    uint16_t word;
    uint32_t dword;

    if (!nox_eud_write_u8(NOX_EUD_DATA_BEGIN, 0xA5u)
        || !expect_u8(NOX_EUD_DATA_BEGIN, 0xA5u))
        return 1;

    if (!nox_eud_write_u16(NOX_EUD_DATA_SPLIT_1 - 1u, 0xB2C3u)
        || !nox_eud_read_u16(NOX_EUD_DATA_SPLIT_1 - 1u, &word)
        || word != 0xB2C3u)
        return 2;

    if (!nox_eud_write_u32(NOX_EUD_DATA_SPLIT_2 - 2u, 0x10203040u)
        || !nox_eud_read_u32(NOX_EUD_DATA_SPLIT_2 - 2u, &dword)
        || dword != 0x10203040u)
        return 3;

    if (nox_eud_read_u8(NOX_EUD_DATA_BEGIN - 1u, (uint8_t *)&word)
        || nox_eud_write_u8(NOX_EUD_DATA_END_EXCLUSIVE, 0x01u)
        || nox_eud_read_u16(NOX_EUD_DATA_END_EXCLUSIVE - 1u, &word)
        || nox_eud_write_u32(0xFFFFFFFFu, 0xFFFFFFFFu))
        return 4;

    /* Normal recovery follows Panic's patched 0x852980 guard. */
    if (!test_recovery_helper(0, 1u, 1))
        return 5;
    if (!test_recovery_helper(0, 0u, 0))
        return 6;

    /* The map-changed recovery helper restores unconditionally. */
    if (!test_recovery_helper(1, 0u, 1))
        return 7;

    /* Heap-backed SpellDB/AbilityDB wide-string pointers round-trip as EUD tokens. */
    if (!test_wide_string_field(NOX_EUD_SPELLDB_NAME_FIELD))
        return 8;
    if (!test_wide_string_field(NOX_EUD_SPELLDB_DESCRIPTION_FIELD))
        return 9;
    if (!test_wide_string_field(NOX_EUD_ABILITYDB_NAME_FIELD))
        return 10;
    if (!test_wide_string_field(NOX_EUD_ABILITYDB_DESCRIPTION_FIELD))
        return 11;

    nox_eud_reset();
    return 0;
}
