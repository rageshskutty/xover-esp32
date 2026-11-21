# ADR 0014: Abstract per-output protection and imported SOS execution

- Status: Proposed; human approval required before implementation or hardware activation.
- Date: 2026-11-22
- Architecture: chain-per-output v1, fixed 48 kHz
- Supersedes: firmware comparator language in ADR 0010/0013 and driver-curve protection authority in ADR 0008.
- Normative schema: configs/schema.json; it is the single source of truth for device JSON fields, enums, and bounds.

## Decision and trust boundary

Firmware contains NO filter-design code. Every filter is imported digital second-order-section (SOS) coefficient data. The firmware executes SOS sections using one coefficient convention and one executor; it does not calculate Butterworth, Linkwitz-Riley, PEQ, shelf, high-pass, low-pass, or protection-weighting coefficients from frequency/Q/gain parameters.

All SOS sections use:

H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2), a0 = 1.

The canonical coefficient names are b0, b1, b2, a1, and a2. No a0 field is transmitted. Coefficients are tied to the one v1 rate, 48,000 Hz. A configuration with another fs_hz, or a firmware executor not running at 48 kHz, is rejected; no runtime resampling or coefficient redesign is allowed.

Firmware and device JSON contain no driver Thiele/Small parameters, driver curves, amplifier/PSU model fields, volts, or watts. Forbidden examples include Fs, Qts, Vas, Re, Le, Xmax, Pe, Sd, derived driver-model fields, amplifier clipping/rail data, and PSU capacity. These are designer-side inputs only.

The designer tool consumes VituixCAD results, driver T/S data, amplifier gain/clipping/rail data, and PSU data. It exports normalized SOS sections, output gain/polarity/delay, and abstract protection fields. No source model or physical-unit value crosses into device JSON, firmware structures, NVS records, network APIs, or firmware validator inputs.

Firmware validates structure, finite values, coefficient bounds, SOS stability, sample rate, section counts, and declared limits only. It does not calculate driver excursion, thermal capacity, amplifier clipping, rail sag, watts, driver curves, or filter coefficients. It does not certify that designer values are physically correct.

## VituixCAD export evidence and importer boundary

The VituixCAD 3.0 help documents export of digital biquad coefficients as b0,b1,b2,a0=1,a1,a2, plus active-buffer gain, delay, and polarity inversion. The active-buffer parameter is expressed in microseconds, but the Copy/Export window's Buffers option reports delay in samples; the EQ-template buffer delay is also documented as rounded to samples. The importer must identify which export path it consumes and verify that value with a golden project rather than assuming the text export uses microseconds. Its DSP-system configuration determines active-filter coefficient behavior and sample rate; Generic provides an adjustable sample rate. The Generic -a12 option reverses the signs of a1/a2 relative to Generic. For this contract the exporter/importer must explicitly select and record the standard Generic convention at 48 kHz, or perform a separately tested conversion from another declared convention. Never infer coefficient sign convention from a file alone.

Only active digital IIR biquads are importable. FIR blocks, transfer-function-file blocks, passive components, bypassed/disabled blocks, and unsupported/non-biquad constructs are not silently approximated; the import tool must reject or explicitly report omissions. VituixCAD documentation states that stability is not checked by its exporter, so stability validation remains mandatory here.

If the importer reads the underlying active-buffer delay parameter, it converts microseconds to integer samples: N = round(t_us * 48000 / 1,000,000). If it consumes VituixCAD's Buffers export or EQ-template delay variable, the documented export value is already rounded to samples and must not be rounded a second time. At 48 kHz, one sample is 20.8333 microseconds (about 7.15 mm of acoustic path). The designer-side report must include requested delay, chosen integer sample count, represented delay, and rounding error in microseconds and equivalent distance. The JSON contains only delay_samples.

References: VituixCAD 3.0 Help, Digital Biquad and Active Buffer / Power amplifier export sections; VituixCAD 2.0 Help, Biquad coefficient export and sample-rate notes. These references establish that export exists, not that every VituixCAD project block is representable by this schema.

## Configuration and signal-path contract

- Root fs_hz is exactly 48000 in v1.
- Each optional input pre entry contains a channel and up to eight SOS sections. This is the runtime user/room-EQ pre-chain.
- Each output has a designer-imported chain object: up to eight SOS sections, gain_db, polarity (+1 or -1), and integer delay_samples (0..4095). Eight sections require 160 bytes of five float32 coefficients plus 64 bytes of two float32 state values per output, excluding alignment/metadata; target memory still requires measurement.
- Every output has one required abstract protection object.
- The output crossover chain is read-only during normal runtime. It can be replaced only by loading and validating a complete configuration, checking storage CRC/integrity, and publishing the complete candidate at the safe configuration boundary. No per-filter, per-coefficient, or partial chain mutation API exists.
- User and room EQ remain in the input pre-chain. The designer's static margin report covers the imported base crossover/output chain only; it does not claim to model arbitrary runtime user-EQ boosts. Those boosts are constrained by the mandatory runtime limiter.

Signal path:

Input → optional input user/room-EQ SOS pre-chain → output mix → imported output SOS crossover → output gain → polarity → integer delay → fixed 5 Hz DC blocker and detector taps → mandatory limiter → system mute/ramp → I2S TDM.

The limiter is the last audio-processing stage. Protection weighting is an SOS cascade used only on the excursion detector path; it never changes programme audio.

## Per-output protection contract

All three thresholds are dBFS referenced to the output-chain signal at the detector tap, after imported crossover, output gain, polarity, delay, and fixed DC blocker. The protection object contains:

| Field | Type / range | Meaning |
|---|---|---|
| peak_ceiling_dbfs | number, -60..0 | Raw output peak ceiling |
| excursion_weighting | SOS array, 0..4 sections | Detector-only weighting cascade |
| excursion_limit_dbfs | number, -60..0 | Limit for peak of excursion-weighted detector signal |
| rms_limit_dbfs | number, -60..0 | Output RMS limit |
| rms_tau_ms | number, 100..60000 | RMS detector integration time constant |
| release_ms | number, 20..5000 | Release time for attenuation recovery |

Fixed firmware constants are not JSON inputs:
- 1 ms lookahead and 1 ms attack;
- 5 Hz DC blocker;
- mute at boot and reset;
- no bypass flag.

The raw peak, excursion-weighted peak, and RMS detectors independently request attenuation. In linear amplitude terms:

- g_peak = min(1, peak_ceiling / peak_detected)
- g_exc = min(1, excursion_limit / excursion_detected)
- g_rms = min(1, rms_limit / rms_detected)
- g_target = min(g_peak, g_exc, g_rms)

Convert dBFS thresholds to linear amplitude before comparison. A zero detector magnitude requests unity gain. Applied gain is constrained to 0..1; no detector may boost. Lookahead and attack are fixed at 1 ms. Release uses release_ms. Every gain change reuses the existing T18 ramp executor and its established per-sample bound/rules; no second ramp implementation is permitted. The final output clamp must not exceed peak_ceiling_dbfs.

Missing, malformed, or out-of-range protection leaves the affected output muted. The system starts muted and may unmute only after every configured output has a valid protection object and the complete candidate has passed validation and safe installation. There is no bypass or permissive default.

## SOS validation and numeric execution

For every section, all five coefficients must be finite and within schema bounds. The semantic validator additionally enforces the strict second-order stability conditions for the canonical denominator convention:

- abs(a2) < 1
- abs(a1) < 1 + a2

The second condition is strict; equality is rejected. Apply it to output crossover SOS, input pre-chain SOS, and protection weighting SOS. Reject the whole candidate on any unstable section. No filter redesign, coefficient clipping, or automatic repair is permitted.

The schema bounds are provisional guardrails, not a claim that all coefficients within those bounds are numerically well-conditioned. The coefficient limits and delay maximum require human review before implementation.

## Validator rules and fail-closed behavior

The schema is normative. Firmware checks structure, finite numeric values, enumerations, coefficient bounds, SOS stability, section counts, delay bounds, and declared limits only.

| Reject condition | Required behavior |
|---|---|
| Invalid JSON, non-object root, duplicate object keys, trailing data, parse/resource limit exceeded | Reject candidate; remain muted or retain current validated configuration |
| Unknown root/output/chain/protection/SOS property | Reject; includes T/S names, driver curves, amp/PSU fields, volts, watts, and bypass |
| Wrong version or fs_hz not exactly 48000 | Reject; no resampling or coefficient conversion in firmware |
| Missing, empty, oversized, unknown, or duplicate input declaration | Reject |
| Missing/empty/oversized outputs; invalid ID; duplicate output ID or TDM slot | Reject |
| Missing/malformed input mix or pre-chain; undeclared/duplicate input channel; non-finite, zero, or out-of-range mix weight | Reject |
| Missing output chain field or invalid gain, polarity, delay, or SOS array | Reject |
| More than eight SOS sections in any pre-chain or output chain; more than four protection-weighting sections | Reject |
| Missing SOS coefficient, extra key, non-finite or out-of-bound coefficient | Reject |
| abs(a2) >= 1 or abs(a1) >= 1 + a2 | Reject entire candidate |
| Missing per-output protection object or required field | Reject; output stays muted |
| Threshold outside -60..0 dBFS; RMS tau outside 100..60000 ms; release outside 20..5000 ms | Reject |
| Attempt to configure lookahead, attack, DC blocker, mute, or bypass | Reject as unknown/forbidden property |
| Candidate cannot be fully validated or safely installed at DSP ownership boundary | Do not publish; retain previous validated config or remain muted |

The JSON Schema defines field names, object closure, numeric bounds, section counts, polarity, and sample rate. The stability inequality, uniqueness/reference checks, parser resource limits, and safe installation are semantic checks. Firmware does not judge acoustic or thermal correctness.

## A/B slot fallback

A/B slots store complete device JSON candidates. Slot header/version/length/CRC checks are storage-integrity checks, not driver-model validation.

1. At boot, keep amplifier mute asserted. Check both slot headers, payload lengths, and CRCs independently.
2. Parse and validate each integrity-valid payload against the current schema and semantic structure/bounds/stability rules.
3. If the selected active slot is fully valid, load it.
4. If the selected slot is invalid and the other slot is fully valid, fall back to the other fully valid slot.
5. If neither is fully valid, or the active-slot selector is invalid, remain muted and report a configuration fault.
6. On update, validate a complete candidate before writing the inactive slot. Write, read back, and verify integrity; only then publish the active selector at a safe boundary. Failed candidate/write/readback must not damage the last known-good slot.

Fallback never relaxes schema, bounds, stability, or mute requirements.

## Designer-side margin and physical mapping (informative; not firmware rules)

The designer owns physical modelling and folds results into abstract dBFS thresholds and detector SOS weighting. These mappings are not schema fields. The static margin report covers only the imported base crossover/output chain and designer-supplied source curves. It does not include arbitrary runtime user/room-EQ pre-chain boosts. Runtime limiter detectors operate after that pre-chain and the output chain, so they remain the protection mechanism for user EQ boosts.

The existing conservative magnitude bound remains:

A_bound(f) = Σ abs(w_i) × abs(H_i(f))

Do not credit phase cancellation. Compare the base-chain bound with designer-supplied abstract limits and report margin by output/frequency. This is an engineering report, not a firmware gate or proof of physical safety.

The designer may derive abstract limits from amplifier clipping, driver thermal capacity, and excursion models. Driver T/S, curves, amplifier gain/clipping/rail data, and PSU data remain outside device JSON and firmware. Consistent peak/RMS conventions, margins, filter normalization, enclosure/port behavior, and source quality require human review.

## Float32 and delay risks / acceptance proposal

### Float32 low-frequency precision

Low-frequency/high-Q sections (for example a 20 Hz high-pass or 80 Hz subwoofer low-pass at 48 kHz) can be sensitive to float32 coefficient quantization and state accumulation. The proposed executor is transposed direct form II (DF2T) in float32. Compare its host-build response and time-domain behavior against an independent double-precision reference, including cascades and long runs.

The proposed response acceptance is:
- firmware host build versus designer-exported response;
- 20 Hz–20 kHz;
- magnitude error within ±0.1 dB and phase error within ±1 degree.

These are proposed limits only, pending measured float32 error and human approval. Record worst-case error over a defined dense frequency grid and representative low-frequency/high-Q cases. If float32 cannot meet the approved limits, evaluate 32-bit fixed-point coefficients/state with 64-bit accumulators as a separate measured design decision; do not silently switch arithmetic.

### Delay rounding

At 48 kHz, one sample is 20.8333 μs, corresponding to approximately 7.15 mm at 343 m/s. Rounding a requested delay to the nearest sample introduces at most half a sample (about 10.4167 μs or 3.57 mm). The designer import report must expose the signed rounding error for each output. Delay is integer-only in device JSON.

## Retired comparator and SciPy oracle

The production C response comparator/API and its firmware gate are retired. Former Gate 3 (C comparator API) and Gate 5 (sourced driver data) are retired as firmware gates and move outside firmware scope.

Keep the independent NumPy/SciPy oracle. Re-scope it as designer-side base-chain margin reporting and as a double-precision numerical reference for C SOS and limiter math tests. It is not linked into firmware and is not a runtime validator. Limiter-specific vectors/tests remain future work and must not be described as implemented.

## Human gates and verification

Remaining human gates:
1. Approve numerical conventions, coefficient sign convention, coefficient bounds, stability implementation, dBFS reference, detector definitions, and excursion-weight normalization.
2. Confirm the exact VituixCAD version/DSP-system profile and importer behavior with a real exported golden project at 48 kHz; confirm all required active filters are represented and unsupported blocks are rejected.
3. Approve the ±0.1 dB / ±1 degree response tolerance after measured float32 results, and decide whether float32 DF2T is sufficient or fixed-point needs a separate evaluation.
4. Approve delay maximum/rounding policy.
5. Select the exact ESP32-S3 module and approve internal SRAM/PSRAM/flash and DSP-state memory budget.
6. Approve ADR 0014 and the schema contract.

No firmware code was changed. No hardware or bench verification was performed. This proposal is not evidence of safe limits or a measured output ceiling.
