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
