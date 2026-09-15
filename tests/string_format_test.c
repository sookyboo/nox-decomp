/* Regression for 2cc50d3: inventory float formatting must be available. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int nox_snprintf(char *str, size_t size, const char *format, ...);

/* Dependencies normally supplied by the platform compatibility layer. */
void compatDebugBreak(void) {}
void compatOutputDebugStringA(const char *message) { (void)message; }
char *compat_itoa(int value, char *buffer, int radix)
{
    static const char digits[] = "0123456789abcdef";
    unsigned int magnitude;
    char *end = buffer;
    int negative = value < 0 && radix == 10;

    if (negative)
        magnitude = (unsigned int)(-(value + 1)) + 1U;
    else
        magnitude = (unsigned int)value;
    do {
        *end++ = digits[magnitude % (unsigned int)radix];
        magnitude /= (unsigned int)radix;
    } while (magnitude != 0);
    if (negative)
        *end++ = '-';
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
    char buf[32];
    int n = nox_snprintf(buf, sizeof(buf), "Mana %.2f", 12.5);
    return (n == 10 && strcmp(buf, "Mana 12.50") == 0) ? 0 : 1;
}
