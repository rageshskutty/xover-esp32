# ADR 0014: Abstract per-output protection and parametric filter stacks

- Status: Proposed; human approval required before implementation or hardware activation.
- Date: 2026-11-22
- Architecture: chain-per-output v1, fixed 48 kHz
- Supersedes: earlier SOS-only/imported-coefficient addenda to this ADR; firmware comparator language in ADR 0010/0013 and driver-curve protection authority in ADR 0008.
- Normative schema: `configs/schema.json`; it is the single source of truth for device JSON fields, enums, and bounds.

## Decision and trust boundary

**JSON is the source of truth and stores parametric filter intent.** Each filter is represented by `type`, `f_hz`, `q`, and `gain_db`. Firmware expands the ordered filter stack into normalized biquad coefficients once, in double precision, while loading a complete configuration. The real-time audio path performs no filter design and no trigonometric/coefficient calculations; it only executes the prepared biquad sections.

This supersedes the previous SOS-only contract. Device JSON does not carry SOS coefficients. There is no VituixCAD coefficient importer in this v1 contract. The designer UI may create/edit the same filter stack as JSON and must plot the calculated response so the designer can compare it visually with a VituixCAD plot. No claim is made that VituixCAD's internal filter definitions or conventions are identical to the definitions below.

Firmware and device JSON contain no driver Thiele/Small parameters, driver curves, amplifier/PSU model fields, volts, or watts. Forbidden examples include Fs, Qts, Vas, Re, Le, Xmax, Pe, Sd, derived driver-model fields, amplifier clipping/rail data, and PSU capacity. These remain designer-side inputs only.

The designer-side calculator consumes driver, amplifier, PSU, and measured gain information and exports only abstract protection values plus parametric detector-weighting filters. No source model or physical-unit value crosses into device JSON, firmware structures, NVS records, network APIs, or firmware validator inputs.

Firmware validates structure, finite values, filter enums and ranges, section counts, sample rate, output gain/polarity/delay, and protection limits. At load time it computes coefficients in double precision, checks that all computed coefficients are finite and stable, converts/prepares the runtime representation, and rejects the complete candidate if any section fails. Firmware does not calculate driver excursion, thermal capacity, amplifier clipping, rail sag, PSU sharing, or physical protection limits.

## Configuration and signal-path contract

- Root `fs_hz` is exactly 48000 in v1. Filter frequency must satisfy `0 < f_hz < 0.45 * fs_hz`.
- Each optional input pre-chain contains an ordered `filters[]` stack with at most eight entries. This is the runtime user/room-EQ pre-chain.
- Each output has a `chain` object with ordered `filters[]` (maximum eight), `gain_db`, `polarity` (+1 or -1), and integer `delay_samples` (0..4095).
- Every output has one required abstract protection object. Its `excursion_weighting` is an ordered array of the same parametric filter objects, maximum four.
- No filter macros or composite filter shortcuts exist in v1. LR4 is represented explicitly as two `lp2` or two `hp2` entries at Q = 0.7071.
- The output crossover chain is read-only during normal runtime. It can be replaced only by loading and validating a complete configuration, checking storage CRC/integrity, and publishing the complete candidate at the safe configuration boundary. No per-filter, per-parameter, or partial chain mutation API exists.
- User and room EQ remain in the input pre-chain. The designer's static margin report covers the base crossover/output chain only; it does not claim to model arbitrary runtime user-EQ boosts. Those boosts are constrained by the mandatory runtime limiter.

Signal path:

Input → optional input user/room-EQ filter pre-chain → output mix → output filter stack → output gain → polarity → integer delay → fixed 5 Hz DC blocker and detector taps → mandatory limiter → system mute/ramp → I2S TDM.

The limiter is the last audio-processing stage. Protection weighting filters operate only on the excursion detector path; they never change programme audio.

## Parametric filter vocabulary and coefficient calculator

Every filter object has exactly `type`, `f_hz`, `q`, and `gain_db`. Bounds are defined only in `configs/schema.json`. The type enum is:

- `lp1`, `hp1`: first-order low-pass and high-pass.
- `lp2`, `hp2`: second-order low-pass and high-pass.
- `peq`: peaking equalizer.
- `lshelf`, `hshelf`: low- and high-shelf.
- `ap1`, `ap2`: first- and second-order all-pass.
- `notch`: second-order notch.

For `lp1`, `hp1`, and `ap1`, Q is a required neutral field fixed at 0.7071 and gain_db is fixed at 0. For `lp2`, `hp2`, `ap2`, and `notch`, gain_db is fixed at 0. For PEQ and shelves, Q and gain_db are active parameters. These canonical neutral values prevent ignored fields from creating multiple JSON encodings for the same filter.

The load-time calculator uses the following fixed convention at `fs = 48000`:

Let `w0 = 2*pi*f_hz/fs`, `c = cos(w0)`, `s = sin(w0)`, `K = tan(pi*f_hz/fs)`, `alpha = s/(2Q)`, and `A = 10^(gain_db/40)`. Compute in double precision and normalize every numerator and denominator coefficient by `a0), so the runtime denominator is `1 + a1*z^-1 + a2*z^-2`.

- `lp1`: `b0=b1=K/(1+K)`, `b2=0`, `a1=(K-1)/(K+1)`, `a2=0`.
- `hp1`: `b0=1/(1+K)`, `b1=-b0`, `b2=0`, `a1=(K-1)/(K+1)`, `a2=0`.
- `ap1`: let `r=(1-K)/(1+K)); `b0=r,b1=1,b2=0,a1=r,a2=0`.
- `lp2`: raw numerator `[(1-c)/2, 1-c, (1-c)/2]`; raw denominator `[1+alpha, -2c, 1-alpha]`.
- `hp2`: raw numerator `[(1+c)/2, -(1+c), (1+c)/2]`; same raw denominator as `lp2`.
- `notch`: raw numerator `[1,-2c,1]`; same raw denominator as `lp2`.
- `peq`: raw numerator `[1+alpha*A,-2c,1-alpha*A]`; raw denominator `[1+alpha/A,-2c,1-alpha/A]`.
- `lshelf`: raw numerator `[A*((A+1)-(A-1)c+2*sqrt(A)*alpha), 2*A*((A-1)-(A+1)c), A*((A+1)-(A-1)c-2*sqrt(A)*alpha)]`; raw denominator `[(A+1)+(A-1)c+2*sqrt(A)*alpha, -2*((A-1)+(A+1)c), (A+1)+(A-1)c-2*sqrt(A)*alpha]`.
- `hshelf`: raw numerator `[A*((A+1)+(A-1)c+2*sqrt(A)*alpha), -2*A*((A-1)+(A+1)c), A*((A+1)+(A-1)c-2*sqrt(A)*alpha)]`; raw denominator `[(A+1)-(A-1)c+2*sqrt(A)*alpha, 2*((A-1)-(A+1)c), (A+1)-(A-1)c-2*sqrt(A)*alpha]`.
- `ap2`: raw numerator `[1-alpha,-2c,1+alpha]`; raw denominator `[1+alpha,-2c,1-alpha]`.

For all second-order types, divide all numerator and denominator terms by the first raw denominator term. The normalized denominator is `[1,a1,a2]`. This defines the product's v1 coefficient convention; it is not a claim of bit-for-bit equivalence with VituixCAD.

At load time, calculate every filter in double precision, check finite values and strict second-order denominator stability, then convert to the chosen runtime representation. Recheck finiteness/stability after float32 conversion if float32 is selected. Reject the whole candidate if a calculated section is non-finite, unstable, or outside implementation-safe numeric limits. Do not clip, repair, or silently skip a filter. The audio task executes the prepared sections only.

For a Linkwitz-Riley fourth-order crossover, enter two second-order Butterworth sections of the same cutoff: two `lp2` or two `hp2` filters, each Q = 0.7071. There are no LR macros or automatic cascades in v1.

## Per-output protection contract

All thresholds are dBFS referenced to the output-chain detector tap after the input pre-chain, output mix, output filter stack, output gain, polarity, delay, and fixed DC blocker. The protection object contains:

| Field | Type / range | Meaning |
|---|---|---|
| peak_ceiling_dbfs | number, -60..0 | Raw output peak ceiling |
| excursion_weighting | filter array, 0..4 entries | Detector-only parametric weighting stack |
| excursion_limit_dbfs | number, -60..0 | Limit for peak of excursion-weighted detector signal |
| rms_limit_dbfs | number, -60..0 | Output RMS limit |
| rms_tau_ms | number, 100..60000 | RMS detector integration time constant |
| release_ms | number, 20..5000 | Release time for attenuation recovery |

Fixed firmware constants are not JSON inputs:
- 1 ms lookahead and 1 ms attack;
- 5 Hz DC blocker;
- mute at boot and reset;
- no bypass flag.

**RMS reference convention:** the RMS detector must report 0 dBFS for a full-scale sine wave. Its calibration therefore includes the sine RMS-to-peak reference; do not define 0 dBFS as the uncorrected RMS value of a peak-full-scale sine (which would read -3.0103 dB). This convention applies to the detector and every RMS threshold/calculation.

The raw peak, excursion-weighted peak, and RMS detectors independently request attenuation. In linear amplitude terms:

- `g_peak = min(1, peak_ceiling / peak_detected)`
- `g_exc = min(1, excursion_limit / excursion_detected)`
- `g_rms = min(1, rms_limit / rms_detected)`
- `g_target = min(g_peak, g_exc, g_rms)`

Convert dBFS thresholds to the corresponding linear detector reference before comparison. A zero detector magnitude requests unity gain. Applied gain is constrained to 0..1; no detector may boost. Lookahead and attack are fixed at 1 ms. Release uses `release_ms`. Every gain change reuses the existing T18 ramp executor and its established per-sample bound/rules; no second ramp implementation is permitted. The final output clamp must not exceed `peak_ceiling_dbfs`.

Missing, malformed, or out-of-range protection leaves the affected output muted. The system starts muted and may unmute only after every configured output has a valid protection object and the complete candidate has passed validation and safe installation. There is no bypass or permissive default.

## Validator rules and fail-closed behavior

The schema is normative. Firmware checks structure, finite values, enumerations, filter parameter bounds, section counts, sample rate, output gain/polarity/delay, and protection ranges. The load-time coefficient calculator validates computed coefficient finiteness and stability.

| Reject condition | Required behavior |
|---|---|
| Invalid JSON, non-object root, duplicate object keys, trailing data, parse/resource limit exceeded | Reject candidate; remain muted or retain current validated configuration |
| Unknown root/output/chain/protection/filter property | Reject; includes T/S names, driver curves, amp/PSU fields, volts, watts, and bypass |
| Wrong version or fs_hz not exactly 48000 | Reject; no resampling |
| Missing, empty, oversized, unknown, or duplicate input declaration | Reject |
| Missing/empty/oversized outputs; invalid ID; duplicate output ID or TDM slot | Reject |
| Missing/malformed input mix or pre-chain; undeclared/duplicate input channel; non-finite, zero, or out-of-range mix weight | Reject |
| More than eight filters in any input pre-chain or output chain; more than four protection-weighting filters | Reject |
| Missing filter field, unknown type, extra key, non-finite value, frequency/Q/gain outside schema bounds, or non-neutral unused field | Reject |
| Load-time coefficient calculation yields non-finite, unstable, or implementation-out-of-range coefficients | Reject entire candidate |
| Missing output chain field or invalid output gain, polarity, or delay | Reject |
| Missing per-output protection object or required field | Reject; output stays muted |
| Threshold outside -60..0 dBFS; RMS tau outside 100..60000 ms; release outside 20..5000 ms | Reject |
| Attempt to configure lookahead, attack, DC blocker, mute, or bypass | Reject as unknown/forbidden property |
| Candidate cannot be fully validated or safely installed at DSP ownership boundary | Do not publish; retain previous validated config or remain muted |

The JSON Schema defines field names, object closure, enums, numeric bounds, section counts, polarity, and sample rate. The load-time calculator's computed-coefficient finite/stability checks, uniqueness/reference checks, parser resource limits, and safe installation are semantic checks. Firmware does not judge acoustic or thermal correctness.

## A/B slot fallback

A/B slots store complete device JSON candidates. Slot header/version/length/CRC checks are storage-integrity checks, not driver-model validation.

1. At boot, keep amplifier mute asserted. Check both slot headers, payload lengths, and CRCs independently.
2. Parse and validate each integrity-valid payload against the current schema and semantic parameter and coefficient rules.
3. If the selected active slot is fully valid, load it.
4. If the selected slot is invalid and the other slot is fully valid, fall back to the other fully valid slot.
5. If neither is fully valid, or the active-slot selector is invalid, remain muted and report a configuration fault.
6. On update, validate a complete candidate before writing the inactive slot. Write, read back, and verify integrity; only then publish the active selector at a safe boundary. Failed candidate/write/readback must not damage the last known-good slot.

Fallback never relaxes schema, parameter bounds, computed coefficient stability, or mute requirements.

## Designer-side protection calculator (informative; not firmware fields)

The following inputs are used only by the designer-side calculator. None is serialized to device JSON or sent to firmware.

| Group | Inputs |
|---|---|
| Driver | Xmax (mm, one-way linear), Pe (W RMS), Z_nom and Z_min (ohms), X_per_V(f) (mm/V peak from the VituixCAD simulation), voice-coil thermal time constant tau (s) |
| Amplifier and PSU | V_clip_pk (V), rail under load, rated amplifier power, PSU rating, PSU efficiency |
| Gain calibration | V0_rms: full-scale sine RMS voltage at the speaker terminals |
| Margins | M_clip = 1 dB, M_th = 3 dB, M_x = 1–2 dB suggested starting values; designer may deliberately set M_x = 0 for the worked example |

Measure V0_rms by playing a -20 dBFS, 1 kHz sine into a suitable dummy load, measuring the speaker-terminal RMS voltage, and multiplying the reading by 10. This is a calibration procedure for the designer tool, not a device input. Then V0_pk = sqrt(2) * V0_rms.

### Amplifier clipping / peak ceiling

`peak_ceiling_dbfs = min(0, 20*log10(V_clip_pk / V0_pk) - M_clip)`

Use the amplifier's peak clipping voltage under the relevant loaded condition. Rail-under-load and rated-power data are designer-side cross-checks; do not serialize them.

### Thermal / RMS limit

`V_rms_max = sqrt(Pe * Z_min)`

`rms_limit_dbfs = min(0, 20*log10(V_rms_max / V0_rms) - M_th)`

The RMS detector reference is calibrated so a full-scale sine reads 0 dBFS. This avoids the 3.0103 dB convention error. The starting `rms_tau_ms` suggestion is about 2000 ms, but this is general guidance only. If the designer adopts the voice-coil thermal time constant `tau_s` directly, convert it as `rms_tau_ms = 1000 * tau_s`; confirm that the chosen detector integration behavior is appropriate for the driver's thermal model. Prefer a datasheet value or measured thermal response. Neither 2000 ms nor a direct one-to-one mapping is a universal constant.

### Excursion limit and detector weighting

`V_allow_min = 0.9 * Xmax / max(X_per_V(f))`

`excursion_limit_dbfs = 20*log10(V_allow_min / V0_pk) - M_x`

`W_target(f) = X_per_V(f) / max(X_per_V(f))`

The normalized target is no greater than 1 (0 dB) at every frequency. Fit it with at most four of the supported parametric filters. The composite weighting response must never sit below the true normalized excursion curve: `abs(W_fit(f)) >= W_target(f)` at every point on the numerical verification grid, while `abs(W_fit(f)) <= 1` everywhere. The designer tool must check this numerically on a sufficiently dense grid, report the worst envelope excess and frequency, and reject a fit that underestimates the curve or exceeds 0 dB. This is a designer-tool check, not a firmware runtime calculation.

Magnitude-only weighting ignores the phase of the true excursion response; resulting waveform-peak error may be around 1–2 dB. M_x is the designer's reserve for that uncertainty, not a proof that phase error is bounded to that value. Sustained bass heating may also exceed a sine-based Pe assumption, so M_th matters.

### Release and supply checks

Suggested release estimate:

`release_ms ~= clamp((5..10) * 1000 / f_low_hz, 20, 5000)`

Choose a multiplier between 5 and 10. For a 20 Hz subwoofer this gives approximately 250–500 ms. This is a starting rule for listening/measurement review, not a mandatory firmware formula.

Designer-side shared supply check:

`sum_over_outputs(V_rms_max_i^2 / Z_min_i) <= PSU_rating * efficiency`

Use the same conservative impedance basis consistently for each output. This check replaces any shared-supply field in firmware. Rail sag, amplifier rated power, and the PSU's continuous/thermal rating remain designer-side cross-checks.

### Worked subwoofer test vector

Inputs: Pe = 300 W, Z_min = 4 ohms, V0_rms = 40 V, V_clip_pk = 50 V, Xmax = 12 mm, max(X_per_V) = 0.8 mm/V, M_clip = 1 dB, M_th = 3 dB, M_x = 0 dB.

- V0_pk = sqrt(2)*40 = 56.5685 V peak.
- peak ceiling = 20*log10(50/56.5685)-1 = -2.07 dBFS, approximately **-2.1 dBFS**.
- V_rms_max = sqrt(300*4) = 34.6410 V RMS.
- RMS limit = 20*log10(34.6410/40)-3 = -4.25 dBFS, approximately **-4.3 dBFS**.
- V_allow_min = 0.9*12/0.8 = 13.5 V peak.
- Excursion limit = 20*log10(13.5/56.5685)-0 = -12.44 dBFS, approximately **-12.5 dBFS**.

These values are a synthetic calculator test vector only, not recommended settings for a real speaker or amplifier. The protection-weighting entries in configs/example_2way_sub.json are also structural examples only; no measured X_per_V curve was supplied and no excursion-envelope fit is claimed for that fixture.

## Float32, response acceptance, and delay risks

### Float32 low-frequency precision

Low-frequency/high-Q sections (for example a 20 Hz high-pass or 80 Hz subwoofer low-pass at 48 kHz) can be sensitive to float32 coefficient quantization and state accumulation. The proposed runtime executor is transposed direct form II (DF2T) in float32. Compare host firmware response from the JSON filter stack against an independent SciPy double-precision oracle and a double-precision time-domain reference, including cascades and long runs.

Proposed response acceptance:
- host firmware biquad response generated from the stacked JSON versus the SciPy oracle;
- 20 Hz–20 kHz;
- magnitude error within ±0.05 dB and phase error within ±0.5 degree.

These are proposals only, pending measured float32 error and human approval. Record worst-case error over a defined dense frequency grid and representative low-frequency/high-Q cases. If float32 cannot meet the approved limits, evaluate 32-bit fixed-point coefficients/state with 64-bit accumulators as a separate measured design decision; do not silently switch arithmetic.

### Delay rounding

At 48 kHz, one sample is 20.8333 microseconds, corresponding to approximately 7.15 mm at 343 m/s. Rounding a requested physical delay to the nearest sample introduces at most half a sample (about 10.4167 microseconds or 3.57 mm). The designer UI/report must show requested delay, chosen integer sample count, represented delay, and signed time/distance rounding error. Device JSON contains integer samples only.

### Manual VituixCAD re-entry risk

No VituixCAD filter coefficient export/import is assumed. A designer manually re-enters filter type, frequency, Q, and gain into the JSON or designer UI. This can introduce transcription errors, and VituixCAD may use different filter definitions, Q conventions, or digital transforms. The designer UI must plot the resulting response and support visual overlay/comparison with the VituixCAD plot. This is a review aid, not a numerical identity claim or automated importer.

## Retired comparator and SciPy oracle

The production C response comparator/API and its firmware gate are retired. Former Gate 3 (C comparator API) and Gate 5 (sourced driver data) are retired as firmware gates and move outside firmware scope.

Keep the independent NumPy/SciPy oracle. Re-scope it as designer-side base-chain margin reporting and as a double-precision numerical reference for the parametric filter coefficient calculator, runtime biquad response, and limiter math tests. It is not linked into firmware and is not a runtime validator. Limiter-specific vectors/tests remain future work and must not be described as implemented. The designer margin report covers the base crossover/output chain only and excludes arbitrary runtime user/room-EQ boosts.

## Human gates and verification

Remaining human gates:
1. Approve the parametric filter coefficient formulas/conventions, especially shelf Q semantics and comparison against VituixCAD plots.
2. Approve the designer protection calculator conventions, RMS full-scale-sine reference, margin values, excursion envelope-fit method, and physical input quality.
3. Approve the ±0.05 dB / ±0.5 degree response tolerance after measured float32 results; decide whether float32 DF2T is sufficient or fixed-point needs separate evaluation.
4. Approve delay maximum/rounding policy and exact UI/report behavior.
5. Select the exact ESP32-S3 module and approve internal SRAM/PSRAM/flash and DSP-state memory budget.
6. Approve ADR 0014 and the schema contract.

No firmware code was changed. No hardware or bench verification was performed. This proposal is not evidence of safe limits or a measured output ceiling.
