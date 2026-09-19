/* Regression for the recovered spell-name lookup stack-buffer layout. */
#include <string.h>

unsigned char byte_5D4594[3844309];
unsigned char byte_587000[400000];

static char resolved_name[64];

int sub_4243F0(const char *name)
{
    if (!name)
        return 0;
    strncpy(resolved_name, name, sizeof(resolved_name) - 1);
    resolved_name[sizeof(resolved_name) - 1] = '\0';
    return 321;
}

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

int sub_51E1D0(const char *name);

static int check_lookup(const char *input)
{
    resolved_name[0] = '\0';
    if (sub_51E1D0(input) != 321)
        return 0;
    return strcmp(resolved_name, "SPELL_SLOW") == 0;
}

int main(void)
{
    strcpy((char *)&byte_587000[253508], "SPELL_%s");

    if (!check_lookup("SLOW"))
        return 1;
    if (!check_lookup("slow"))
        return 2;
    if (!check_lookup("Slow"))
        return 3;
    return 0;
}
