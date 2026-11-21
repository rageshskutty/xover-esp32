# Design

Status: PROPOSED. Items marked VERIFY must be confirmed against a datasheet,
the source of a pinned dependency, or a measurement before code relies on
them. ADRs 0012 and 0013 define the proposed chain-per-output architecture
and protection boundary.

## 1. Fixed facts and provisional platform choices

| Item | Value | Status |
|---|---|---|
| Sample rate | 48 kHz | v1 product requirement |
| Initial system | 2.1, stereo input, six output channels | product requirement |
| Future layout | 5.1 and other multichannel layouts | architecture requirement |
| TDM frame | 8 slots x 32 bit | design choice; VERIFY hardware |
| BCLK | 48000 x 8 x 32 = 12.288 MHz | arithmetic, conditional on TDM format |
| MCLK | 256 x fs = 12.288 MHz | design choice; VERIFY PCM5122 clocking |
| DSP block | 64 frames = 1.333 ms | provisional design choice |
| DSP core | core 1; control/network work on core 0 | design choice |
| FPU | single precision in hardware; double is software | ESP32-S3 platform property |
| Audio source | ESP32-audioI2S hook candidate | dependency/version/license VERIFY |

The six initial outputs are left woofer, left tweeter, right woofer, right
tweeter, sub A, and sub B. Exact physical slot assignment remains a hardware
bring-up item.

## 2. Signal path and chain execution

Conceptual flow:

```text
Input L/R
  -> optional input pre-chain (shared room correction / tilt)
  -> per-output input mix (explicit channels and weights)
  -> ordered per-output user DSP chain
  -> compiler-appended thermal/RMS, excursion and peak protection
  -> global supply power budget
  -> final clamp
  -> system ramp / mute
  -> TDM output
```

There is no user-authored DAG, edge list, topological sort, or general-purpose
graph executor. Each output has an explicit `in` mix and an ordered `chain`.
An optional `pre` chain belongs to an input channel and may feed multiple
outputs. Each output's stateful user and protection stages have independent
state. The input pre-chain is shared only where the configuration explicitly
declares it.

The core-0 builder validates the complete JSON and compiles each output into
a bounded table of precompiled stage function pointers, immutable parameters,
and allocated state. The DSP task executes the table in declared order.
No JSON parsing, coefficient design, heap allocation, logging, locks, or
unbounded loops occur in the real-time path.

Existing `dspblocks` primitives are reused where their contracts and tests
remain valid. The graph-oriented `dspgraph` build/runtime model is retired.
A new `dspchain` component replaces its role. Exact public C interfaces,
table layout, capacities, state ownership, and memory requirements must be
frozen before implementation.

## 3. Filter conventions

- Transfer function:
  `H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2)`,
  a0 = 1. This is the scipy.signal convention.
- DF2T:
  `y = b0 x + z1`; `z1 = b1 x - a1 y + z2`;
  `z2 = b2 x - a2 y`.
- VituixCAD export: its "Generic" and "Generic -a12" options differ in the
  sign of a1/a2. Which one matches this convention must be verified by a
  golden vector before import is enabled.
- Design uses the bilinear transform with frequency pre-warping, so the
  analog -3.01 dB point lands exactly on fc.
- Butterworth order N: ceil(N/2) sections (an odd order has one first-order
  section stored as a biquad with b2 = a2 = 0).
- Linkwitz-Riley order N (even): two cascaded Butterworth filters of order
  N/2. Sections = N/2. LR2 = two first-order sections = one biquad.
- LR summing: for N mod 4 = 0 (LR4, LR8), LP and HP branches sum to an
  all-pass in phase. For N mod 4 = 2 (LR2, LR6), one branch must be polarity
  inverted to sum flat. Firmware does not silently add this inversion.
- Bessel: normalization convention must be approved against VituixCAD.
  Until decided, Bessel is rejected as E_UNSUPPORTED.
- RBJ cookbook conventions apply to PEQ, low/high shelf, notch and all-pass.

## 4. User DSP stage catalog

The following are ordinary user-chain stages, subject to the schema's
required parameters and bounds:

| Stage | Purpose | Notes |
|---|---|---|
| biquad (`k=lp`, `hp`, `peq`, `notch`, `allpass`) | designed second-order filter | `f_hz` and `q`; PEQ also requires `db` and may carry optional `role` |
| sos | imported coefficients | `fs`-tagged; finite, stable, and equal to 48000 Hz |
| gain | level trim | `db`; bounded; no hidden auto gain |
| delay | whole-sample delay | `us`; bounded by existing DSP primitive |
| polarity | explicit polarity inversion | `invert`; user-declared only |

Input mixing is declared in each output's `in` field as source-channel and
linear-weight pairs; it is not a standalone graph node. Runtime protection
stages are compiler-owned and cannot appear in the ordinary `chain`. A
limiter placed in a user chain is invalid.

JSON v1 is closed and defined in `configs/schema.json`: top-level
`v: 1`, `fs: 48000`, declared `inputs`, optional `pre`, and 1–8
`outputs`. Each output has unique `id` and `slot`, 1–8 unique `in`
channels, and an ordered `chain`. Mix weights are finite, non-zero and in
[-4,4], including negative weights. `pre` is unique by channel; undeclared
channels are rejected. A channel without a pre-chain is identity. `protect`
accepts only optional `attack_ms`, `release_ms`, and reserved
`lookahead_ms: 0`. See ADR 0012 and ADR 0013 for semantic and protected-record
checks.

## 5. Chain semantics and resource bounds

- Each input may have an optional pre-chain for shared room correction/tilt.
- Each output declares an explicit ordered input mix and ordered DSP chain.
- Mix order is preserved from JSON. No implicit channel duplication,
  summing, polarity inversion, delay alignment, or phase compensation occurs.
- Pre-chain state is per input. Each output chain owns independent filter
  state; one output's processing must not advance another output's state.
- Each compiled stage has a known function, parameters, and allocated state.
  Processing walks the output's table in order.
- The initial section cap is 96 SOS sections, provisional until measured on
  target. The cap must account for the worst per-output chain expansion, not
  merely the sum of unique filter definitions.
- Existing DSP code, test vectors, and golden response fixtures are retained
  where compatible. Graph-only concepts such as cycle detection and
  fan-in-without-mix are retired and replaced by chain-order and input-mix
  validation tests.

## 6. Protection model (fail closed)

### Provisioned authority

Device protection is derived from a separately provisioned protected block:
`drv` driver data, `amp` amplifier data, and `supply` power-supply data.
These values are not part of uploaded speaker JSON and cannot be changed by
the phone UI or network API. Each output has a provisioned frequency-dependent
limit curve.

### Static load-time validation

For each output, evaluate each input-channel-to-output transfer, including
the input pre-chain and output user chain. If the mix weights are `w_i` and
the transfer magnitudes are `|H_i(f)|`, calculate:

```text
A_bound(f) = sum_i(abs(w_i) * abs(H_i(f)))
```

Compare this conservative bound against the output's provisioned limit
curve. Do not credit phase cancellation between stereo inputs. Include all
static user filters and gains. Runtime limiter behavior is not a substitute
for passing the static check.

The initial evaluation grid uses the existing 1054-point logarithmic grid
from 10 Hz to 20 kHz plus exact provisioned curve knots and required model
boundaries. The final point limit and interpolation rules are frozen in the
comparator prototype; no extrapolation may silently weaken protection.

The current chain-v1 schema has no `auto_trim` field. Automatic trim is
disabled in v1: the compiler must reject an over-limit candidate and must
never change the user's gain. A future auto-trim feature requires a separate
human-reviewed schema and algorithm decision.

The required subsonic high-pass is a structural requirement, not proof of
attenuation. The static response evaluator must also calculate the actual
input-to-output response at the protected `min_freq_hz` and across the full
evaluation grid, then compare it with the provisioned curve. The temporary
presence-only guardrail is not accepted as safety evidence; until the
response check is integrated into the configuration validator, protected
static acceptance fails closed.

### Mandatory runtime protection

The compiler appends the required stages after the user chain:
1. thermal/RMS limiter;
2. sealed-box excursion limiter when required by the provisioned model;
3. peak limiter;
4. system-wide supply power budget;
5. final hard clamp;
6. system-controlled ramp and mute.

Runtime limiters are gain-only and may attenuate but never boost. Thermal
detector parameters and limiter thresholds come only from the protected
block. The designer may select only `protect.attack_ms` and
`protect.release_ms`, each bounded by protected `lim_bounds`; omission
uses protected `lim_default`. Non-zero `lookahead_ms` is rejected in v1.

For a sealed-box driver model, excursion protection uses a single low-pass
biquad derived from `fc_hz`, `Qtc`, and `mm_per_v`. Missing, unsupported,
unstable, or non-finite model data fails closed. A required subsonic high-pass
stage must be present in the user chain.

The final clamp guarantees `abs(y) <= configured_ceiling` for every sample.
The hardware volume/gain setting is pinned. Before enabling output, validate
`vpk_at_0dbfs <= clip_v` using provisioned amplifier data. Missing or
invalid values keep outputs muted.

## 7. State machine and configuration lifecycle

```text
BOOT_MUTED -> PROTECTED (valid drv/amp/supply)
           -> CONFIG_VALID (schema + static protection pass)
           -> RAMP_UP -> RUN
RUN -> RAMP_DOWN -> APPLY_CANDIDATE -> RAMP_UP -> RUN
any state -> FAULT_MUTED
```

All start, stop, configuration-change, and fault transitions use controlled
ramps. Down-ramp is at least 5 ms and up-ramp at least 20 ms, subject to the
existing system safety policy. No output step may exceed -60 dBFS.

Configuration mutations are serialized through one transaction gate. A
concurrent mutation returns `E_BUSY` before candidate build or storage
writes. The DSP owner acknowledges activation at a safe boundary. The
proposed 250 ms acknowledgement timeout remains provisional and requires
human approval.

Persistence occurs only on an explicit user commit; previews and slider
movement must not write flash. The exact distinction between preview, live
apply, and persistent commit is a human-reviewed interface decision.
`FAULT_MUTED` is currently assumed to remain latched until power-cycle and
fresh validation; this is provisional and requires human safety review.

## 8. Real-time architecture and budgets

| Item | Rule / value |
|---|---|
| DSP task | pinned to core 1, highest app priority, stack in internal SRAM |
| Block | 64 frames; 1.333 ms at 48 kHz (provisional) |
| Input ring | SPSC, fixed capacity in internal SRAM |
| Core 0 | Wi-Fi, lwIP, HTTP, decoder, validation, design, compile, logging |
| Memory | coefficients, stage tables, filter state, ring, and DMA in internal SRAM |
| Double precision | core 0 only |
| DSP task | no heap, logging, locks, JSON, double, recursion, or unbounded loops |
| Underruns | 0 in a 1 h soak target |
| DSP noise floor | <= -110 dBFS unweighted 20 Hz-20 kHz target |
| Swap click | no step above -60 dBFS target |
| Allowed blocking calls in RT | I2S write and documented ring read |

The 96-section cap and timing targets are not measured feasibility claims.
The target report must record worst-case per-output chain cost, cycles,
execution-time percentiles, memory high/low water, stack use, DMA footprint,
and underruns with Wi-Fi active. A 64-frame block is 1.333 ms; DSP execution,
full-block service, and deadline margin must be reported separately.

## 9. Hardware (VERIFY against datasheets)

- PCM5122 serial audio port TDM/DSP mode and data offset register must be
  verified from the exact board and datasheet before use.
- Three DACs need distinct I2C addresses or an approved I2C mux; verify straps.
- MCLK source and clock configuration must be verified.
- Exact ESP32-S3 module, flash/PSRAM, GPIO map, DAC slot mapping, amplifier
  mute behavior, and power-supply limits are human-owned.
- Startup keeps DAC soft-mute and amplifier mute asserted, starts I2S with
  zeros, validates protection/configuration, then ramps up.

## 10. Security

LAN-only control with bearer token for writes, constant-time comparison,
bounded request size/depth, and no firmware outbound traffic beyond the
approved API. Unknown JSON keys are rejected. Protected data is never
writable through the network. Flash writes occur only on explicit user
commit and must obey a human-approved endurance budget.
