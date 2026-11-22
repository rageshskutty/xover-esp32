# Design

Status: PROPOSED. Hardware facts marked VERIFY require a datasheet, pinned
dependency source, or measurement before implementation relies on them.
ADR 0012 defines chain-per-output execution; ADR 0014 defines protection.

## 1. Platform

| Item | Value | Status |
|---|---|---|
| Sample rate | 48 kHz | v1 requirement |
| Initial system | 2.1, stereo input | product requirement |
| Future layout | 5.1 and other multichannel layouts | architecture requirement |
| TDM frame | 8 slots x 32 bit | design choice; VERIFY hardware |
| BCLK | 12.288 MHz at 8 x 32-bit slots and 48 kHz | arithmetic; VERIFY format |
| DSP block | 64 frames = 1.333 ms | provisional |
| DSP core | core 1; control/network work on core 0 | design choice |

Output slot assignment, DAC clocking, module memory, GPIO map, and amplifier
mute wiring remain hardware bring-up items.

## 2. Configuration and execution

JSON is the source of truth. configs/schema.json defines v1: v, fs, declared
inputs, optional per-input pre[].filters, outputs[], and optional top-level
protection.outputs. Each output declares its input mix, ordered filters[],
gain, polarity, and sample delay. Every filter has type, f_hz, q, and gain_db.

At configuration load, firmware validates the document and expands each
parametric filter into biquad section(s) once. The real-time path executes
compiled sections in declared order; it does not parse JSON, design
coefficients, allocate, log, lock, or run unbounded loops. Existing dspblocks
primitives are reused where their numerical and parameter contracts match.
The dspchain execution-table layout, ownership, capacities, and public API
must be frozen before runtime implementation.

A missing input pre-chain is identity. Mix and filter order are preserved.
Each output owns independent state. No implicit routing, summing, polarity
change, delay alignment, or phase compensation is added.

## 3. Filter stack

Allowed types: lp1, hp1, lp2, hp2, peq, lshelf, hshelf, ap1, ap2, notch.
The schema owns frequency, Q, gain, and section-count bounds. Output stacks
are limited to eight entries; detector weighting to four.

First-order types accept the common q field but ignore it. Second-order
filters use declared Q. PEQ and shelf gain uses gain_db; other types do not
apply it. Filter design happens once during configuration load.

SOS convention: H(z) = (b0 + b1 z^-1 + b2 z^-2) /
(1 + a1 z^-1 + a2 z^-2), a0 = 1, matching SciPy. Do not regenerate existing
response vectors merely to make an implementation pass.

## 4. Signal path and protection

Per output: input mix; ordered filter stack; output gain, polarity, and
delay; peak/RMS detection in parallel; lookahead attenuation and final
digital ceiling clamp; system-owned ramp/mute and output.

ADR 0014 and configs/schema.json define protection. Records contain
detector-weighting filters, peak and RMS thresholds, RMS time constant,
release, and final sample ceiling. Weighting applies only to peak detection.
RMS is unweighted because it observes the crossover-band-limited output.

The limiter uses fixed 1 ms lookahead (48 samples at 48 kHz), not a JSON
field. Its gain is attenuation-only. The final clamp enforces the digital
sample ceiling, not loudspeaker excursion or thermal safety. Weighting-filter
group delay can exceed lookahead at low frequencies; excursion margin and
infrasonic design remain the speaker designer's responsibility. Firmware
must not claim that the clamp guarantees Xmax.

A staging configuration may omit top-level protection but must remain muted.
Outputs may be enabled only after valid protection covers every configured
output and the DSP owner installs the validated state at a safe boundary.
Boot, reset, failed validation, missing protection, or incomplete coverage
leaves hardware muted.

## 5. Known low-frequency limitation

The 20 Hz schema minimum applies to output and detector-weighting filters.
It supports typical subwoofer tunings around 25 Hz and above, but a
lower-tuned ported sub cannot express a weighting corner below 20 Hz. The
float32 response test measured 0.0706 dB error at 10 Hz against the 0.01 dB
gate, so the gate was held and the schema minimum moved to 20 Hz. If lower
corners are needed, improve executor conditioning or precision and meet the
same accuracy gate before changing the schema. Fixed-point is only a future
investigation.

## 6. Real-time and hardware boundaries

Core 0 owns Wi-Fi, HTTP, parsing, validation, filter design, compilation, and
storage. Core 1 owns bounded DSP and TDM output. The audio path has no heap,
JSON, logging, locks, double precision, or unbounded loops. The DSP task owns
live state allocated before real-time processing. Startup asserts DAC and
amplifier mute and transmits zeros until validation.

Measure worst-case chain cost, execution percentiles, memory high/low water,
stack, DMA footprint, and underruns with Wi-Fi active. The block and section
limits are not target feasibility claims until measured. PCM5122 TDM mode,
I2C addresses/straps, MCLK, pins, exact module, amplifier mute, DC blocking,
and fuse/PTC selection require human/datasheet verification. Use a safe
dummy load or sacrificial driver for initial bench work.
