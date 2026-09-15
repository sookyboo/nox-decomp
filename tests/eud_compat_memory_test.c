#include <stdint.h>

#include "../src/eud_compat.h"

#define NOX_EUD_DATA_BEGIN 0x00581450u
#define NOX_EUD_DATA_SPLIT_1 0x00587000u
#define NOX_EUD_DATA_SPLIT_2 0x005D4594u
#define NOX_EUD_DATA_END_EXCLUSIVE 0x0097EE69u

static int expect_u8(uint32_t address, uint8_t expected)
{
    uint8_t value = 0;
    return nox_eud_read_u8(address, &value) && value == expected;
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

    return 0;
}
