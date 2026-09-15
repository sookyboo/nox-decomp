/* Regression for 1301939: preserve IEEE-754 bits in sub_52E210. */
#include <stdint.h>
#include <string.h>

static uint32_t round_trip(uint32_t bits)
{
    float value;
    uint32_t result;
    memcpy(&value, &bits, sizeof(value));
    memcpy(&result, &value, sizeof(result));
    return result;
}

int main(void)
{
    static const uint32_t cases[] = {
        0x00000000u, /* +0 */
        0x80000000u, /* -0 */
        0x3f800000u, /* 1.0 */
        0xbf800000u, /* -1.0 */
        0x00000001u, /* smallest subnormal */
        0x007fffffu, /* largest subnormal */
        0x7fc00001u, /* quiet NaN */
        0x7f800000u, /* +inf */
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        if (round_trip(cases[i]) != cases[i])
            return (int)i + 1;
    return 0;
}
