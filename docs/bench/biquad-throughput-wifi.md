# Biquad throughput under Wi-Fi load on ESP32-S3 (N16R8)

Status: **UNVERIFIED** under AGENTS V1 until the maintainer records the run in
`docs/bringup-log.md`. The logs were produced by the maintainer on real hardware; an agent
analysed them and never writes HW-VERIFIED. This is a micro-benchmark of the biquad kernel with
the radio and TCP stack busy on core 0. It is not the ADR 0009 measurement of a real output chain.

This follows up `biquad-throughput.md` (same benchmark, no Wi-Fi). Read that report first for the
method and the no-load numbers.

Raw data (MAC addresses replaced by `xx:xx:xx:xx:xx:xx`, nothing else edited):

| File | What it is |
|---|---|
| `biquad-throughput-wifi-2026-11-28.log` / `.csv` | Wi-Fi build with a TCP client attached: the load run |
| `biquad-throughput-wifi-2026-11-28-client.log` | PC-side throughput printed by `wifi_load_client.py`, one line per second |
| `biquad-throughput-wifi-idle.log` / `.csv` | Same binary, AP up, no client (`WIFI_TRAFFIC,NO_CLIENT`). The log holds two boots; the CSV is the first. Control run |
| `biquad-throughput-2026-11-27.csv` | No-Wi-Fi build, from `biquad-throughput.md` |

Tool: `tools/biquad-bench/` built with `sdkconfig.wifi` (artifact `biquad-bench-wifi-esp32s3`).

## Setup
| Item | Value |
|---|---|
| Board | ESP32-S3 N16R8 (rev v0.2), 16 MB flash, 8 MB octal PSRAM at 80 MHz |
| Firmware | app version 4735170, compile time Nov 27 2026 18:13:51 (as printed at boot), ESP-IDF v5.5.6. The CI run id was not recorded |
| Placement | bench task on core 1; Wi-Fi driver task, lwIP and the traffic tasks on core 0 |
| Radio | SoftAP `xover-bench`, 802.11 bgn, 40 MHz upper (as logged for the station), power save off, TCP MSS 1440, TCP window 5760 |
| Load | `tools/biquad-bench/wifi_load_client.py` from one Windows laptop: one TCP connection, send and receive at full speed |
| Method | Same as `biquad-throughput.md`: 20 warm-up blocks, 2000 timed blocks per row, 208 rows. One run per condition |
| Run date | 2026-11-28, maintainer's local date (UTC+05:30) |
| Power | USB. Cable length not recorded for this run |

The benchmark started after `WIFI_TRAFFIC,YES` (client joined, three seconds at or above
256 kB/s) and ended with `BENCH_STATUS,OK` and `BENCH_DONE`. All correctness checks in the log
PASS with the same values as the no-Wi-Fi run (coefficients 1.2e-7, cascade 3.4e-5, LPF
attenuation 42.74 dB against 42.74 dB analytic).

### Traffic during the benchmark
The firmware logs `WIFI,<ms>,<rx kB>,<tx kB>,<clients>` once per second. Over the 146 samples
between `WIFI_TRAFFIC,YES` and `BENCH_DONE` (about 148 s, one client throughout):

| Direction (board side) | Median | Mean | Min | Max |
|---|---:|---:|---:|---:|
| Received from the laptop, kB/s | 613 | 597 | 239 | 696 |
| Sent to the laptop, kB/s | 845 | 821 | 327 | 949 |

The client log is consistent with that (mean 582 kB/s to the board and 800 kB/s from it over 199 non-zero
seconds). The rate was not constant. The client log shows four dips of two to five seconds each,
at client seconds 64 to 68, 133 to 137, 169 to 170 and 198 to 199, where throughput to the board
fell to between 270 and 430 kB/s (normal is about 600). On the board side one second (239 kB/s)
was below the 256 kB/s start threshold. This log does not show what caused the dips (laptop
radio, channel, or the benchmark itself), so the cycle results below include them.

For scale, 48 kHz 16-bit stereo PCM is 192 kB/s (48000 x 2 x 2 bytes). The inbound rate here is
about three times that. The traffic is a stress load, not a model of the audio stream.

## Method of comparison
Rows are paired on (scenario, impl, mem, channels, block, biquads per channel). Percent changes
are `load / idle - 1` on cycle counts. "Idle" is the first boot of `biquad-throughput-wifi-idle.log`,
same binary, AP up, no client. Statistics use the 112 six-channel rows (six channels is the design
configuration; the other 96 rows are two-channel).

Control checks on the idle run:

| Comparison (112 six-channel rows) | Median change in mean cycles | Largest |
|---|---:|---:|
| Idle Wi-Fi build against the no-Wi-Fi build | +0.10 % | +0.52 % |
| Second idle boot against the first | +0.00 % | +0.05 % |

So having the Wi-Fi code in the image and the AP idle costs almost nothing, and the benchmark
repeats to within a few hundredths of a percent. The differences below come from the traffic.

## Findings
1. **Traffic costs a few percent, not tens.** Across the 112 six-channel rows, mean cycles rose by
   a median 2.2 % (range 0.7 to 5.3 %), p99 by a median 3.8 % (1.7 to 8.8 %) and the worst block by
   a median 4.7 % (1.9 to 25.5 %).
2. **No block exceeded its budget.** The highest worst-block load in any row is 56.5 %
   (`esp_dsp_ansi`, 6 ch, block 32, 16 biquads per channel). For `ref_c` the highest is 39.8 %
   (same shape). No row has a worst block above 100 % of the block period.
3. **Small blocks suffer more.** For `ref_c`, 6 ch, 16 biquads per channel the mean rose 4.6 % at
   block 32, 3.2 % at 64, 2.4 % at 128 and 1.9 % at 256; p99 rose 7.5, 4.9, 3.4 and 2.5 %. The
   cause was not isolated by this run.
4. **The largest percentage jumps are in the cheapest rows.** The top worst-block increases are
   +25.5 % (`esp_dsp_aes3`, 6 ch, block 32, 1 biquad, 4795 to 6018 cycles) and +23.5 % (2 biquads);
   in absolute terms those rows peak at 3.8 % and 6.5 % of the period.
5. **`ref_c` still matches the assembly kernel.** Across the 28 six-channel `ref_c` rows,
   `esp_dsp_ae32` is between 1.4 % faster and 0.5 % slower on mean cycles under load (0.0 to 0.2 %
   faster when idle). `esp_dsp_ansi` is 43 to 53 % slower under load (median 50 %), as in the no-load
   report.
6. **PSRAM buffers still cost nothing measurable.** 6 ch, block 256, 8 biquads: `ref_c` in PSRAM
   is 0.65 % cheaper than internal under load (0.00 % idle). The buffers are small, so this says
   little about heavy PSRAM traffic.

### Load of one core, 6 channels, `ref_c`, idle to Wi-Fi load
| Biquads per channel | Block | Mean | p99 | Worst |
|---|---:|---:|---:|---:|
| 8 | 32 | 18.0 to 18.9 % | 18.6 to 20.1 % | 18.6 to 20.7 % |
| 8 | 64 | 17.2 to 17.8 % | 17.5 to 18.5 % | 17.6 to 18.8 % |
| 8 | 128 | 16.8 to 17.2 % | 16.9 to 17.6 % | 17.1 to 17.7 % |
| 8 | 256 | 16.6 to 16.9 % | 16.6 to 17.1 % | 16.7 to 17.2 % |
| 16 | 32 | 36.0 to 37.7 % | 36.5 to 39.3 % | 37.1 to 39.8 % |
| 16 | 64 | 34.4 to 35.5 % | 34.6 to 36.3 % | 35.0 to 36.6 % |
| 16 | 128 | 33.6 to 34.4 % | 33.6 to 34.8 % | 33.9 to 34.9 % |
| 16 | 256 | 33.2 to 33.8 % | 33.2 to 34.1 % | 33.3 to 34.1 % |

`lr4_eq` scenario, `ref_c`, 6 ch, block 256 (2 biquads for the crossover plus 0, 4 or 8 EQ biquads):

| Sections | Mean | p99 | Worst |
|---|---:|---:|---:|
| 2 | 4.2 to 4.2 % | 4.2 to 4.4 % | 4.2 to 4.5 % |
| 6 | 12.4 to 12.7 % | 12.5 to 12.9 % | 12.6 to 12.9 % |
| 10 | 20.8 to 21.1 % | 20.8 to 21.4 % | 20.9 to 21.4 % |

## What this means for the design (arithmetic on the data above)
- The provisional cap of 96 sections (`docs/design.md` section 5) at the 64-frame block went from
  34.4 % mean and 35.0 % worst block (idle) to 35.5 % mean and 36.6 % worst block under this
  load. That is 1.1 and 1.6 points. This data does not by itself call for changing the cap.
- At block 32 the same configuration reaches 39.8 % worst block, 2.6 points above idle. If the
  design moves to 32-frame blocks, use the load column, not the idle column.
- The largest increase in any six-channel row is 5.3 % on mean cycles and 8.8 % on p99. The
  25.5 % worst-block growth seen in the cheapest rows is large in percent only (3.8 % of the
  period at most).

## Not covered
- The ADR 0009 target report: full-block service, deadline margin, instrumentation overhead, 1 h
  underrun soak, SRAM low-water. This run had no I2S and no audio.
- One run per condition. The idle control repeated to hundredths of a percent, but the load run
  has laptop and radio variability (the four throughput dips) and has not been repeated.
- SoftAP with one TCP connection and a PC client, not a station joined to a router, not HTTP,
  not a real audio stream (UDP or RTP framing, jitter buffer, decoder) and not config uploads.
- What causes the extra cycles (shared memory bus, cache, interrupts) and what causes the
  throughput dips. This log cannot separate them.
- The other blocks in a real chain (gain, delay, limiter, protection, mix, float conversion).
- Brownout margin. The first report recorded `BROWNOUT_RST` on a 1.5 m cable. This run did not
  brown out, but the cable length was not noted.

## Entry for `docs/bringup-log.md` (maintainer to review and paste)
```
- Date: 2026-11-28
- Firmware commit: 4735170 (tools/biquad-bench, sdkconfig.wifi build)
- Gate / task: BENCH-BIQUAD follow-up, Wi-Fi load (design.md section 5 section cap, not a safety gate)
- Setup (amps/speakers disconnected? yes/no): <maintainer to confirm>; no I2S, no DAC driven
- Instrument and settings: on-chip cycle counter, 240 MHz, 2000 blocks per row, bench task on core 1;
  SoftAP, one TCP client at about 0.6 MB/s in and 0.8 MB/s out on core 0
- Measured values: 6 ch x 16 biquads, ref_c, block 64: 34.4 % mean / 35.0 % worst idle,
  35.5 % / 36.6 % under load; six-channel median mean-cycle increase 2.2 %; no row above 56.5 % worst-block load
- Result: PASS (HW-VERIFIED) for the kernel-under-Wi-Fi-load measurement only
- Notes: control run with AP up and no client matches the no-Wi-Fi build within 0.52 %; four throughput dips of 2 to 5 s during the run
```
