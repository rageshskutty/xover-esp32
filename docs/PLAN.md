# PLAN

> **STATUS: RETIRED LEGACY PLAN — NOT AN EXECUTABLE TASK LIST**
>
> This file describes the former graph/DAG task sequence (including T04,
> T06, and T07) and is retained for historical context only. It is
> superseded for current product architecture and task sequencing by
> ADRs 0012/0013 and `docs/engineering/task_state.json`. Do not assign,
> execute, or generate 7B task cards from the legacy T00–T15 cards below.
> In particular, the old graph-based T06/T07/T09 instructions are retired.
>
> Current work follows the parametric filter-stack contract in configs/schema.json and ADR 0014. Schema-derived task cards and the T16–T22 implementation sequence are next. See docs/engineering/task_state.json for task state.
>
> The old G0–G6 criteria are not proof that the chain-v1 architecture has
> been implemented or qualified. Do not use their historical task IDs or
> acceptance criteria as current authority.
>
> ---
>
Rules for working on this plan are in AGENTS.md. Design is in docs/design.md,
contracts in docs/api.md. Take the lowest-numbered unblocked task. One task =
one branch = one PR.

## 1. Human decisions (agents do not decide these)
| id | decision | blocks |
|---|---|---|
| D1 | ESP32-S3 module: flash size, PSRAM | T01, T08, T10 |
| D2 | ESP-IDF version (5.x) | T00b, T08 |
| D3 | arduino-esp32 version that pairs with D2 | T10 |
| D4 | GPIO map: MCLK, BCLK, WS, DOUT, SDA, SCL (+ amp mute GPIO if any) | T01, T08 |
| D5 | PCM5122 board model and address straps | T08 |
| D6 | Driver protection limits per output | T07, S3 cap lift |
| D7 | Repository licence (after the ESP32-audioI2S licence check) | T15 |
| D8 | Phone streaming scope beyond URL/radio playback | T10 |
| D9 | Config swap method: mute-ramp-swap (default) or crossfade | T06 |

## 2. Dependency graph
    T00 -> T01 -> T08 -> T09 -> T10 -> T11 -> T12 -> T14 -> T15
    T00 -> T02 -> T03 -> T04 -> T05 -> T06 -> T07 -> T09
                                     T03 -> T13
T02-T07 and T13 need no hardware and can run in parallel with T01/T08 waiting on
the human.

## 3. Gates (all criteria must pass; hardware items are entered by the human)
| Gate | After | Pass criteria |
|---|---|---|
| G0 | T00, T01 | `python tools/check.py` green on main; CI green; board_config.h filled by the human; D1, D2, D4, D5 answered |
| G1 | T08 | HW: all three PCM5122 answer on I2C; BCLK and MCLK = 12.288 MHz +/-0.01%; six distinct tones, one per slot, each slot within +/-0.5 dB of the others at equal level; crosstalk <= -80 dB; no start/stop step above -60 dBFS; level capped at -30 dBFS, amps disconnected |
| G2 | T02-T05 | HOST: golden and analytic tests pass: Butterworth -3.01 dB +/-0.005 at fc, LR4 -6.02 dB +/-0.005 at fc, |LP+HP| = 0 dB +/-0.001 for LR4/LR8; design response within 0.001 dB and 0.01 deg of scipy sosfreqz over 10 Hz-20 kHz; float32 execution within 0.01 dB of double above 20 Hz; line coverage >= 90% on dspblocks; ASan and UBSan clean |
| G3 | T06, T07 | HOST: valid configs load; every configs/invalid/* is rejected with its _expect code; 100000 mutated configs: no crash, no hang, each <= 50 ms; a config violating a protection record is rejected E_PROTECT; fail-closed with no NVS record |
| G4 | T09 | TARGET: block time p99.9 <= 0.60 ms and max <= 1.00 ms over 1 h, 6 channels, Wi-Fi active; 0 underruns; DSP noise floor <= -110 dBFS unweighted; swap click <= -60 dBFS; measured cycles per section recorded and the section cap in check.py re-derived |
| G5 | T10-T12 | TARGET: 1 h of 48 kHz FLAC over HTTP, 0 underruns; POST /config round trip <= 1 s; 100 consecutive swaps with no step above -60 dBFS; free internal heap stable within 1 KB; /response agrees with the host evaluation within 0.01 dB |
| G6 | T13-T15 | VituixCAD golden import passes; docs complete; licence set; release tag |

## 4. Risks
| risk | mitigation |
|---|---|
| Three PCM5122 on one TDM bus (slot offsets, addresses) | T08 first on hardware; fallback: two data lines on the two I2S ports |
| float32 noise at 70 Hz | measured gate G4; escalation ladder in design.md section 8 |
| CPU budget | cost cap in check.py, measured in T09, 48 kHz only |
| Arduino/IDF version pairing for ESP32-audioI2S | D3; build spike in T10 before integration |
| Decoder pacing after taking over output | blocking push in audiosrc, tested in T10 |
| Protection check misses a narrow peak | grid 1/96 octave, peq q <= 20, ADR for residual risk |
| VituixCAD sign or Bessel conventions | golden vector test in T13; Bessel rejected until decided |
| Swap clicks | mute-ramp-swap, G5 measurement |
| Agent guesses hardware facts | S1, `#error` templates, docs/questions.md |

## 5. Task cards
Format: goal; files; out of scope; acceptance; size (S < 100 lines, M < 250,
L <= 400). Evidence for every acceptance item goes in the PR.

### T00 - Guardrails (provided in this package)
Goal: AGENTS.md, docs, tools/check.py, invalid configs, CI, PR template, CODEOWNERS.
Acceptance: `python tools/check.py` passes on a clean checkout; CI green. Human:
enable branch protection with required CI and code-owner review.
### T00b - IDF CI job (after D2)
Goal: add an `idf.py build` job in an espressif/idf container pinned to D2.
Files: .github/workflows/ci.yml. Acceptance: job builds the skeleton. Size S.
### T01 - Board config template (after D1, D4, D5)
Goal: firmware/main/board_config.h with every pin and address as a named macro;
each unset macro triggers `#error`. The human fills the values. Also add Kconfig
XOVER_BRINGUP_MAX_DBFS (default -30).
Out of scope: any driver code. Acceptance: with values filled the skeleton builds;
with one removed the build fails with a clear message. Size S.
### T02 - Host build and biquad engine
Goal: host/CMakeLists.txt building dspblocks without IDF; `dsp_sos_process` in
dsp_sos_rt.c (DF2T float); minimal test harness (no external deps); CI host job.
Files: firmware/components/dspblocks/{include/dspblocks.h, dsp_sos_rt.c},
host/CMakeLists.txt, host/tests/test_sos.c, .github/workflows/ci.yml.
Acceptance: impulse response of a known biquad equals the hand-computed values
to 1e-6; in-place processing identical to out-of-place; check.py RT scan clean.
Size M.
### T03 - Filter design and golden vectors
Goal: `dsp_design_*` (Butterworth, Linkwitz-Riley, peq, shelf, all-pass, notch)
in double; tools/gen_golden.py (pinned numpy/scipy versions) writes
host/tests/golden/*.json; tests compare coefficients and responses.
Out of scope: Bessel. Acceptance: G2 design items. Size L (split by family if needed).
### T04 - Response evaluation
Goal: `dsp_sos_eval` and a log-grid helper; compare with scipy sosfreqz.
Acceptance: magnitude within 0.001 dB, phase within 0.01 deg, 10 Hz-20 kHz, 3
filter families. Size M.
### T05 - Simple blocks and numeric safety
Goal: gain (dB with ramp), polarity, integer-sample delay line, mix; NaN/Inf
detection per block output; denormal handling (flush or tiny offset), verified
with a 10 s decaying tail. Files: dspblocks/{blocks_rt.c,...}. Acceptance: delay
exact to the sample, gain ramp monotonic, NaN input never reaches the output
stage. Size M.
### T06 - Graph build, validate, swap (needs D9)
Goal: `dspg_build` per docs/api.md: JSON parse (cJSON, MIT, pinned, same source
on host and target), schema-equivalent checks, semantic rules from tools/check.py,
variable resolution, topological sort, static pool in internal SRAM, section cap,
issue report. Engine submit/process with pointer swap at a block boundary and
ramps. Tighten configs/schema.json per node type. Freeze docs/api.md (ADR).
Acceptance: G3 items except protection; ASan/UBSan clean; every invalid config
returns its code; the example config builds and `dspg_eval` of sub_lp and
l_woofer_lp match scipy. Size L (split: parser+validator, then engine).
### T07 - Protection (after D6 for real values)
Goal: protection check per design.md section 6; tools/provision_protect.py writing
an NVS image (uses ESP-IDF nvs_partition_gen); fail-closed behaviour; unit tests
with synthetic records. Out of scope: real driver numbers (human, D6).
Acceptance: G3 protection items; a config that widens a limit is still rejected;
no network path can write the records. CODEOWNERS path. Size M.
### T08 - TDM output and PCM5122 bring-up (after G0)
Goal: tdmout (IDF 5 i2s_tdm.h, 8x32 bit, 48 kHz, MCLK) and pcm5122 (I2C init,
soft-mute, per-chip slot offset from datasheet), plus a `main.c` bring-up mode
playing a distinct sine per slot at <= -30 dBFS with a mute ramp. Writes
docs/bringup-procedure.md with the exact measurements for G1 (the human runs
them and fills docs/bringup-log.md). Acceptance: builds; G1 by the human. Size L.
### T09 - DSP task, ring, timing
Goal: core-1 DSP task, audiosrc ring (SPSC), cycle counter instrumentation
(`esp_cpu_get_cycle_count`), stats, fault handling, state machine of design.md
section 7, sine source feeding the graph with the example config.
Acceptance: G4 (human runs the soak); check.py RT scan clean; timing assertions
for every RT function. Size L.
### T10 - Streaming source (after D3, D8)
Goal: add ESP32-audioI2S as a submodule pinned to a reviewed commit; build spike
for the IDF + arduino-esp32 pairing; hook sets `*continueI2S = false` and calls
`audiosrc_push`; assert 48 kHz; blocking backpressure; docs/licenses.md entry.
Acceptance: G5 streaming item. Size L.
### T11 - HTTP API (xoweb)
Goal: docs/api.md section 4 exactly; token auth (constant time); body limits;
rate limit; `/response` from `dspg_eval`; config storage in xocfg slots with
CRC and fallback. Tests: Python client against the host-built handlers for
validation paths; device tests listed for the human. Acceptance: G5 upload and
response items. Size L (split storage from handlers).
### T12 - Phone page
Goal: web/index.html + small JS, no external CDN, <= 50 KB gzipped: file picker,
upload with token, `sub_xo` selector limited to the config limits, response plot
(canvas), status line. Acceptance: works on a phone browser offline from the
device (human); no network requests except the device. Size M.
### T13 - VituixCAD import
Goal: tools/vituixcad.vxt template and tools/import_vituixcad.py that turns exported
biquads into `biquad` nodes with sample_rate_hz = 48000; resolves the a1/a2 sign
convention with a golden vector; decides Bessel normalisation (ADR with the human).
Acceptance: G6 import item. Size M.
### T14 - Qualification
Goal: soak scripts and docs/qualification.md: 1 h streaming, 100 swaps, heap
trend, noise floor, click measurement procedure; update the section cap from
measured cycles. Human runs, agent writes procedures and analysis scripts.
Acceptance: all gates G4-G5 recorded in bringup-log.md. Size M.
### T15 - Release
Goal: README final, licence (D7), user guide for the phone page and for
changing drivers (edit JSON, upload), changelog, tag v0.1.0. Size S.

## 6. Prompt template for any agent
    You are working in https://github.com/rageshskutty/xover-esp32.
    Read AGENTS.md, docs/PLAN.md, docs/design.md, docs/api.md first.
    Your task: <T-id and title>. Do only that task card.
    Follow every hard rule in AGENTS.md. Never guess hardware facts; log
    questions in docs/questions.md and stop that part. Write tests first.
    Before opening the PR run `python tools/check.py` (and the host tests once
    they exist). The PR must use the template, paste real command output as
    evidence, and label every result HOST-VERIFIED, HW-VERIFIED or UNVERIFIED.
    Do not merge, do not push to main, do not change a gate to make it pass.

## 7. Review checklist for the human
1. Does the PR touch only the files of its task card?
2. Is every number backed by pasted output?
3. Did a hard rule (S/R/C/V/X) get waived? Is the waiver justified?
4. Any CODEOWNERS path touched? Is the rejecting test present?
5. Did the PR change a gate or contract without an ADR?
6. For hardware gates: have you run the procedure and entered the log yourself?
