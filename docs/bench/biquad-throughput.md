# Biquad throughput on ESP32-S3 (N16R8)

Status: **UNVERIFIED** under AGENTS V1 until the maintainer records the run in
`docs/bringup-log.md`. The log was produced by the maintainer on real hardware; an agent
analysed it and never writes HW-VERIFIED. This is a micro-benchmark of the biquad kernel. It is
not the ADR 0009 measurement of a real output chain.

Raw data: `biquad-throughput-2026-11-27.log` (full serial capture) and
`biquad-throughput-2026-11-27.csv` (the `CSV,` rows). Tool: `tools/biquad-bench/`. Binary built
by the `Firmware binaries` workflow (`.github/workflows/firmware-binaries.yml`).

## Setup
| Item | Value |
|---|---|
| Board | ESP32-S3 N16R8 (QFN56, rev v0.2), 16 MB flash (boya), 8 MB octal PSRAM at 80 MHz |
| CPU | 240 MHz, both cores; bench task pinned to core 1 |
| ESP-IDF / esp-dsp | v5.5.6 / 1.8.2 (`DSPS_BIQUAD_F32` expands to `dsps_biquad_f32_ae32`) |
| App version | 5d51949 (built by CI, run 45992927111) |
| Method | 20 warm-up blocks, then 2000 timed blocks per row with `esp_cpu_get_cycle_count()`; yields every 64 blocks; no Wi-Fi |
| Load figure | cycles / (block period x 240 MHz), one core, biquads only |
| Power | 75 cm data cable, laptop on its charger. A 1.5 m phone cable gave `BROWNOUT_RST` boot loops at about 0.6 s; that run produced no data |

Correctness checks in the same log all PASS: coefficients max diff 1.2e-7, single stage 6.9e-7,
8-stage cascade 3.4e-5 (all three esp-dsp kernels), LPF attenuation 42.74 dB measured against
42.74 dB analytic.

## Findings
1. `ref_c` (hand-written C) and `esp_dsp_ae32` / `esp_dsp_aes3` (assembly) cost the same,
   within 0.1 cycle per biquad-sample. `esp_dsp_ansi` costs about 52 % more. The repo can keep its
   own C kernel and needs no esp-dsp dependency for biquads (AGENTS X4).
2. Cost per biquad per sample is identical for 2 and 6 channels, so load scales linearly with the
   total section count.
3. Larger blocks are cheaper per sample: fixed overhead is about 8 % at 32 samples, 3 % at 64.
4. Placing the audio buffers in PSRAM changed nothing (6 ch, 256 samples, 8 biquads: 16.59 % in
   both). The buffers are small enough to stay in cache, so this says little about heavy PSRAM
   traffic.
5. Timing is stable: p99 is within 0.3 points of the mean at block 256. The widest
   worst-case-versus-mean gap in any sweep row is 0.7 points.

### Cycles per biquad-sample (mean of the sweep rows)
| Block | `ref_c` | `esp_dsp_ae32` | `esp_dsp_ansi` |
|---|---:|---:|---:|
| 32 | 18.7 to 19.3 | 18.6 to 19.2 | 27.4 to 28.0 |
| 64 | 17.9 | 17.9 | 26.8 |
| 128 | 17.5 | 17.5 | 26.5 |
| 256 | 17.3 | 17.3 | 26.3 |

### Load of one core, 6 channels, `ref_c` (mean / p99)
| Biquads per channel | Block 32 | Block 64 | Block 128 | Block 256 |
|---|---:|---:|---:|---:|
| 8 | 18.0 / 18.6 % | 17.2 / 17.5 % | 16.8 / 16.9 % | 16.6 / 16.6 % |
| 16 | 35.9 / 36.4 % | 34.3 / 34.5 % | 33.6 / 33.6 % | 33.2 / 33.2 % |

`lr4_eq` scenario (6 ch, block 256, 2 biquads for the crossover plus 0, 4 or 8 EQ biquads):
2 sections 4.2 %, 6 sections 12.4 %, 10 sections 20.7 % (`ref_c`; `esp_dsp_ansi` 6.3, 18.9, 31.5 %).

## What this means for the design (arithmetic on the data above)
- The provisional cap of 96 SOS sections (`docs/design.md` section 5) costs about 34.3 % of one
  core at the 64-frame block, regardless of how the sections are spread over the six outputs,
  because cost depends on the total. At block 64 that is 109.9 k cycles of a 320 k-cycle block.
- The ceiling for biquads alone is about 48 sections per channel at 100 % load (5000 cycles per
  sample frame, 6 x 17.3 per section), so roughly 290 sections in total. Plan for well under 50 %.

## Not covered
- Everything ADR 0009 asks of a target report: full-block service, deadline margin, instrumentation
  overhead, Wi-Fi active, 1 h underrun soak, SRAM low-water. This run had no Wi-Fi and no I2S.
  The kernel under Wi-Fi load is in `biquad-throughput-wifi.md`.
- The other blocks in a real chain (gain, delay, limiter, protection, mix, float conversion).
- Float32 precision of long cascades (ADR 0016); this log only shows agreement with the reference.
- Contention between core 0 (Wi-Fi, streaming) and the core 1 DSP task on cache and the PSRAM bus.

## Entry for `docs/bringup-log.md` (maintainer to review and paste)
```
- Date: 2026-11-27
- Firmware commit: 5d51949 (tools/biquad-bench, built by CI run 45992927111)
- Gate / task: BENCH-BIQUAD (design.md section 5 section cap, not a safety gate)
- Setup (amps/speakers disconnected? yes/no): yes, USB power only, no DAC, no amp
- Instrument and settings: on-chip cycle counter, 240 MHz, 2000 blocks per row, bench task on core 1
- Measured values: ~17.3 cycles per biquad-sample at block 256 (17.9 at block 64); 6 ch x 16 biquads
  = 34.3 % of one core at block 64; ref_c equals esp_dsp ae32; log in docs/bench/
- Result: PASS (HW-VERIFIED) for the kernel throughput measurement only
- Notes: 1.5 m cable caused BROWNOUT_RST loops; 75 cm cable fixed it
```
