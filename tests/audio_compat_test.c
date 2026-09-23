/* Regression for 2755c9b: ADPCM boundary blocks must not overflow or index
 * outside the IMA step/index tables. */
#define _XOPEN_SOURCE 700
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

unsigned int nox_test_decode_adpcm(int16_t *out, const unsigned char *data,
                                   unsigned int size, int stereo);
int nox_test_decode_pcm_stream(const char *filename, int16_t *out,
                               unsigned int max_samples);
int nox_test_pcm_stream_file_size(const char *filename);
int nox_test_pcm_stream_seek(const char *filename, unsigned int position);
int nox_test_mp3_seek_resets_state(unsigned int *buffered,
                                   unsigned int *chunk_pos,
                                   unsigned int *chunk_size);

typedef struct SDL_mutex SDL_mutex;
void SDL_LockMutex(SDL_mutex *mutex) { (void)mutex; }
void SDL_UnlockMutex(SDL_mutex *mutex) { (void)mutex; }

void alGenSources(int count, unsigned int *sources)
{
    while (count-- > 0)
        *sources++ = 1;
}

void alGenBuffers(int count, unsigned int *buffers)
{
    while (count-- > 0)
        *buffers++ = 1;
}

int alGetError(void) { return 0; }
const char *alGetString(int error) { (void)error; return ""; }
FILE *compat_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

static int samples_are_clipped_safely(const int16_t *samples, unsigned int count)
{
    unsigned int i;

    for (i = 0; i < count; ++i) {
        if (samples[i] < INT16_MIN || samples[i] > INT16_MAX)
            return 0;
    }
    return 1;
}

static void put32(unsigned char *dst, unsigned int value)
{
    dst[0] = (unsigned char)value;
    dst[1] = (unsigned char)(value >> 8);
    dst[2] = (unsigned char)(value >> 16);
    dst[3] = (unsigned char)(value >> 24);
}

static int test_pcm_stream_at_riff_boundary(void)
{
    unsigned char wav[120] = {0};
    char filename[] = "/tmp/nox-audio-test-XXXXXX";
    int fd;
    int16_t decoded[2];
    ssize_t written;
    int samples;
    int file_size;

    memcpy(wav, "RIFF", 4);
    put32(wav + 4, sizeof(wav) - 8);
    memcpy(wav + 8, "WAVEfmt ", 8);
    put32(wav + 16, 16);
    wav[20] = 1;             /* PCM */
    wav[22] = 1;             /* mono */
    put32(wav + 24, 8000);   /* sample rate */
    put32(wav + 28, 16000);  /* byte rate */
    wav[32] = 2;             /* block alignment */
    wav[34] = 16;            /* bits per sample */
    memcpy(wav + 36, "JUNK", 4);
    put32(wav + 40, 68);     /* places data header at old file-size limit */
    memcpy(wav + 112, "data", 4);
    put32(wav + 116, 0);     /* final zero-length chunk at old file-size limit */

    fd = mkstemp(filename);
    if (fd < 0)
        return 0;
    written = write(fd, wav, sizeof(wav));
    close(fd);
    if (written != (ssize_t)sizeof(wav)) {
        unlink(filename);
        return 0;
    }

    samples = nox_test_decode_pcm_stream(filename, decoded, 2);
    file_size = nox_test_pcm_stream_file_size(filename);
    if (!nox_test_pcm_stream_seek(filename, 0)) {
        unlink(filename);
        return 0;
    }
    unlink(filename);
    return samples == 0 && file_size == (int)sizeof(wav);
}

static int test_mp3_loop_seek_resets_decoder_state(void)
{
    unsigned int buffered;
    unsigned int chunk_pos;
    unsigned int chunk_size;

    if (!nox_test_mp3_seek_resets_state(&buffered, &chunk_pos, &chunk_size))
        return 0;
    return buffered == 0 && chunk_pos == 0 && chunk_size == 0;
}

int main(void)
{
    unsigned char mono[2048];
    unsigned char stereo[2050];
    int16_t decoded[8192];
    unsigned int count;

    if (!test_pcm_stream_at_riff_boundary())
        return 1;
    if (!test_mp3_loop_seek_resets_decoder_state())
        return 1;

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
