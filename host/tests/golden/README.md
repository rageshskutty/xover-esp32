# MP3 golden vectors

Real MP3 streams plus the reference decode, for the future `mp3dec` component
(host test reads them from `MP3DEC_GOLDEN_DIR`). Do not edit by hand; changing
them changes what the decoder is graded against, so treat a regeneration as a
deliberate decision, not a test fix.

- `*.mp3`: encoded with libmp3lame via ffmpeg 6.1.1-3ubuntu5, no Xing/LAME tag,
  no ID3 (`-write_xing 0 -id3v2_version 0`), 0.35 s of synthetic signal
  (tones, chirp, 5 noise bursts that force short blocks, L/R differing).
- `*.pcm`: reference decode by ffmpeg's `mp3` decoder, s16le interleaved,
  exactly frames x samples-per-frame samples (no delay trimming needed).
- `manifest.json`: name, sample rate, channels, file sizes.
- Generator: `tools/gen_mp3_vectors.py OUTDIR` (needs ffmpeg with libmp3lame and
  numpy). A regenerated set may not be byte-identical across ffmpeg/LAME versions.

Coverage (parsed from the side info): long, short and start/stop blocks, scfsi
reuse, preflag, M/S joint stereo, subblock gain; MPEG-1 (32/44.1/48 kHz,
mono/stereo, CBR 64..320 kbps, VBR), MPEG-2 (16/22.05/24 kHz), MPEG-2.5 (8/11.025 kHz).
NOT covered: mixed blocks, intensity stereo, free-format, CRC frames (LAME does
not emit them here). Those need hand-built tests.
