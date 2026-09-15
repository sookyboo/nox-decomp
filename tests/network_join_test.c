/* Regression for 6a6f369: joining must obtain the serial prerequisite used by
 * the network handshake from the platform registry path. */
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include "../src/windows.h"
#endif

#ifdef _WIN32
#define TEST_WINAPI WINAPI
#else
#define TEST_WINAPI
#endif

unsigned char byte_5D4594[3844309];
unsigned char byte_587000[400000];

static int queried;
static char serial[32];

LONG TEST_WINAPI RegOpenKeyExA(HKEY root, LPCSTR subkey, DWORD options, REGSAM access, PHKEY result)
{
    (void)root; (void)options; (void)access;
    if (!subkey || strcmp(subkey, "SOFTWARE\\Westwood\\Nox") != 0)
        return 2;
    *result = (HKEY)(uintptr_t)0x1234;
    return 0;
}

LONG TEST_WINAPI RegQueryValueExA(HKEY key, LPCSTR value, LPDWORD reserved, LPDWORD type,
                                  LPBYTE data, LPDWORD size)
{
    const char expected[] = "1234567890123456789012";
    (void)reserved;
    if (key != (HKEY)(uintptr_t)0x1234 || strcmp(value, "Serial") != 0 ||
        !data || !size || *size < sizeof(expected))
        return 2;
    memcpy(data, expected, sizeof(expected));
    memcpy(serial, data, sizeof(expected));
    *size = sizeof(expected);
    *type = 1; /* REG_SZ */
    queried = 1;
    return 0;
}

LONG TEST_WINAPI RegCloseKey(HKEY key)
{
    return key == (HKEY)(uintptr_t)0x1234 ? 0 : 2;
}

int sub_420120(unsigned char *out);

int main(void)
{
    unsigned char out[32] = {0};

    memcpy(byte_587000 + 60144, "Serial", sizeof("Serial"));
    if (!sub_420120(out))
        return 1;
    if (!queried || strcmp((char *)out, serial) != 0)
        return 2;
    return 0;
}
