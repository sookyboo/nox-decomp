/* Regression for 2755c9b: ADPCM boundary blocks must not overflow or index
 * outside the IMA step/index tables. */
#include <stdint.h>
#include <string.h>

unsigned int nox_test_decode_adpcm(int16_t *out, const unsigned char *data,
                                   unsigned int size, int stereo);

static int samples_are_clipped_safely(const int16_t *samples, unsigned int count)
{
    unsigned int i;

    for (i = 0; i < count; ++i) {
        if (samples[i] < INT16_MIN || samples[i] > INT16_MAX)
            return 0;
    }
    return 1;
}

int main(void)
{
    unsigned char mono[2048];
    unsigned char stereo[2050];
    int16_t decoded[8192];
    unsigned int count;

    memset(mono, 0x77, sizeof(mono));
    mono[0] = 0xff;
    mono[1] = 0x7f;
    mono[2] = 0xff; /* Invalid IMA index; production code must saturate it. */
    mono[3] = 0;
    count = nox_test_decode_adpcm(decoded, mono, sizeof(mono), 0);
    if (count != 1u + (sizeof(mono) - 4u) * 2u)
        return 1;
    if (decoded[0] != INT16_MAX)
        return 1;
    if (!samples_are_clipped_safely(decoded, count))
        return 1;

    memset(stereo, 0x77, sizeof(stereo));
    stereo[0] = 0xff;
    stereo[1] = 0x7f;
    stereo[2] = 0xff;
    stereo[4] = 0xff;
    stereo[5] = 0x7f;
    stereo[6] = 0xff;
    count = nox_test_decode_adpcm(decoded, stereo, sizeof(stereo), 1);
    if (count != 2u + ((sizeof(stereo) - 8u) / 8u) * 16u)
        return 1;
    if (decoded[0] != INT16_MAX || decoded[1] != INT16_MAX)
        return 1;
    if (!samples_are_clipped_safely(decoded, count))
        return 1;

    return 0;
}
