/* Regression for ac10013: cut-scene chunk assembly must not overflow its
 * fixed 0x800-byte scratch buffer when one resource chunk is oversized. */
#include <stdint.h>
#include <string.h>

unsigned char byte_5D4594[3844309];
unsigned char byte_587000[400000];

static unsigned char oversized_chunk[2300];
static int released;

int **sub_420A90(int ***stream, int *length)
{
    (void)stream;
    if (released)
        return 0;
    *length = (int)sizeof(oversized_chunk);
    return (int **)(void *)oversized_chunk;
}

int sub_420940(int stream, int chunk, int length, int mode)
{
    (void)stream;
    (void)chunk;
    (void)length;
    (void)mode;
    released = 1;
    return 0;
}

unsigned char *sub_40F120(int stream, uint32_t *length);

int main(void)
{
    uint32_t length = 0;
    unsigned char *result;

    memset(oversized_chunk, 0xA5, sizeof(oversized_chunk));
    memset(byte_5D4594, 0, sizeof(byte_5D4594));
    released = 0;

    result = sub_40F120(0, &length);

    if (result != byte_5D4594 + 207988)
        return 1;
    if (length != 0x800u || !released)
        return 2;
    if (memcmp(result, oversized_chunk, 0x800u) != 0)
        return 3;
    return 0;
}
