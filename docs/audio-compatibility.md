# Audio compatibility backend

`src/compat_mss.c` implements the Miles/AIL compatibility layer used by the
game's digital samples and streams. `AIL_load_sample_buffer` supplies sample
buffers, and the timer callback reaches `sample_work`, which decodes ADPCM
blocks and queues them to OpenAL. `AIL_open_stream` selects the PCM, IMA
ADPCM, or MP3 decoder from the WAV format and `stream_work` queues decoded
frames.

The IMA ADPCM decoders (`decode_adpcm` and `decode_adpcm_stereo`) consume the
block header followed by low-then-high nibbles. The initial predictor and index
come from each channel's header; the index is clamped to the 0..88 IMA table
range before it is used, and each decoded predictor is clamped to signed
16-bit audio. Stereo blocks are processed only while a complete eight-byte
channel pair remains. These details are based on the current implementation;
the original game's exact malformed-block behavior is uncertain.

The `2755c9b` regression test uses boundary-sized synthetic mono and stereo
blocks with invalid initial indexes and a trailing partial stereo block. It
calls the production decoder through a test-only build hook and verifies the
sample count and signed-16-bit output range. The sample path additionally
limits ADPCM blocks to 2048 bytes and caps queued decoded samples to the
destination buffer; the stream path tracks padded RIFF chunk positions so
successive chunks are decoded from the correct offset.

The `e68bba7` coverage in the same test opens a synthetic PCM WAV through
`AIL_open_stream`. Its `data` chunk begins exactly where the pre-fix RIFF
file-size calculation stopped searching, so the test verifies the `size + 8`
RIFF bound and processes the final zero-length chunk through the PCM decoder.
MP3 channel-count reporting and OpenAL queue/lifecycle behavior still require
an integration fixture with a real decoder/device path.

On native 64-bit builds, the Miles digital-driver handle is authoritative in
the host-width `nox_mss_digital_handle` sidecar. The recovered DWORD at
`byte_5D4594[816432]` remains the i386 storage location and a compatibility
copy only on native builds. `sub_43E8E0()`, `sub_43E910()`, `sub_43EA20()`,
`sub_43EC10()`, `sub_43F130()`, `sub_43F140()`, and `sub_43F1A0()` must use the
sidecar before passing the driver to AIL/OpenAL lifecycle calls; reading the
native handle from the recovered slot truncates it and can crash during the
startup movie/audio transition.

The `743f879` coverage drives public `AIL_set_stream_position` on a synthetic
MP3 stream state. The seek lifecycle resets the minimp3 decoder and clears
buffered compressed bytes plus the current RIFF chunk position and size, which
forces the next decode to rediscover the chunk from the loop start. Actual
decoded-frame continuity and audible gaplessness still require a real MP3
fixture and OpenAL integration.
