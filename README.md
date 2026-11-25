# xover-esp32

Config-driven active crossover for a 2.1 speaker system.
ESP32-S3 + 3x PCM5122 over I2S TDM. DSP runs on core 1 at 48 kHz.

Precompiled DSP blocks live in firmware. A JSON config wires blocks together and sets parameters, so changing speakers does not require rebuilding firmware.

See docs/requirements.md, docs/architecture.md, docs/audio-source.md, and docs/hw-pinmap.md. The 8 MB partition layout in firmware/partitions.csv is provisional: verify the actual module flash size before flashing.

## Status
The architecture is chain-per-output (ADR 0012 and ADR 0014; both are Proposed).
The DSP block library and host tests are implemented; the firmware has no chain executor, firmware protection evaluator or JSON loader (the host-side config checker is separate).
`idf.py build` is currently configured for the bring-up modes; target-build CI remains red on the known baseline.
Do not connect speakers: outputs remain fail-closed and bring-up signals are limited to -30 dBFS until human verification (AGENTS S2 and S3).
The live task state is tracked in `docs/engineering/task_state.json`.

## Build order
1. TDM bring-up: three PCM5122 DACs and six-channel sine test.
2. DSP block engine and host tests against scipy.
3. Streaming source integration.
4. JSON loader, protection checks, and web UI.
5. VituixCAD export tooling.

## Quick start
```sh
cd firmware
idf.py set-target esp32s3
idf.py build
```
