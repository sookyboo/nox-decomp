/* Regression for 3302bad: inventory durability values are integer bit
 * patterns even though the decompiler typed their temporaries as floats. */
#define NOX_INVENTORY_DURABILITY_TEST
#include "../src/GAME2.c"

#include <string.h>

void compatDebugBreak(void) {}
void compatOutputDebugStringA(const char *message) { (void)message; }

char *compat_itoa(int value, char *buffer, int radix)
{
    static const char digits[] = "0123456789abcdef";
    unsigned int magnitude = (unsigned int)value;
    char *end = buffer;

    do {
        *end++ = digits[magnitude % (unsigned int)radix];
        magnitude /= (unsigned int)radix;
    } while (magnitude != 0);
    *end = '\0';

    for (char *left = buffer, *right = end - 1; left < right; ++left, --right) {
        char tmp = *left;
        *left = *right;
        *right = tmp;
    }
    return buffer;
}

int main(void)
{
    static const struct {
        int current;
        int maximum;
        const wchar_t *expected;
    } cases[] = {
        {0, 100, L"Durability: 0 / 100"},
        {1, 100, L"Durability: 1 / 100"},
        {99, 100, L"Durability: 99 / 100"},
        {100, 100, L"Durability: 100 / 100"}
    };
    wchar_t output[64];
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        nox_test_format_durability(output, cases[i].current, cases[i].maximum);
        if (nox_wcscmp(output, cases[i].expected) != 0)
            return 1;
    }
    return 0;
}
