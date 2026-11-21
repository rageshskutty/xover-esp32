# ADR 0014: Abstract per-output protection block

- Status: Proposed; human approval required before implementation or hardware activation.
- Date: 2026-11-22
- Architecture: chain-per-output v1
- Supersedes: firmware comparator language in ADR 0010/0013 and driver-curve protection authority in ADR 0008.
- Normative schema: configs/schema.json. This is the single source of truth for device JSON fields, enums, and bounds.

## Decision and trust boundary

Firmware and device JSON contain no driver Thiele/Small parameters, driver curves, amplifier/PSU model fields, volts, or watts. Forbidden examples include Fs, Qts, Vas, Re, Le, Xmax, Pe, Sd, derived driver-model fields, amplifier clipping/rail data, and PSU capacity. These are designer-side inputs only.

The designer tool consumes VituixCAD results, driver T/S data, amplifier gain/clipping/rail data, and PSU data. It converts them into the abstract protection fields below. No source model or physical-unit value crosses into device JSON, firmware structures, NVS records, network APIs, or firmware validator inputs.

Firmware validates structure and declared bounds only. It does not calculate driver excursion, thermal capacity, amplifier clipping, rail sag, watts, or driver curves, and it does not certify that the designer's values are physically correct. The designer owns correctness, margins, normalization, and suitability. Schema acceptance means only that the exported document is structurally admissible.

## Per-output protection contract

Every output object has one required protection object. There is no separate driver/amp/supply record and no top-level protected-record table.

All three thresholds are dBFS referenced to the signal at the end of that output's user chain, after crossover, user PEQ, gain, delay, and polarity:

| Field | Type / range | Meaning |
|---|---|---|
| peak_ceiling_dbfs | number, -60..0 | Raw output peak ceiling |
| excursion_weighting | array, 0..4 biquad specs | Detector-only weighting cascade |
| excursion_limit_dbfs | number, -60..0 | Limit for peak of excursion-weighted detector signal |
| rms_limit_dbfs | number, -60..0 | Output RMS limit |
| rms_tau_ms | number, 100..60000 | RMS detector integration time constant |
| release_ms | number, 20..5000 | Release time for attenuation recovery |

Each excursion filter is an object with type, f_hz, q, and conditional gain_db. Type is one of hp, lp, peq, lshelf, hshelf. gain_db is required for peq/lshelf/hshelf and forbidden for hp/lp. The schema closes every object with additionalProperties:false. Weighting applies only to the excursion detector path; it never changes programme audio.

Firmware constants are not JSON inputs:
- 1 ms lookahead and 1 ms attack;
- 5 Hz DC blocker;
- mute at boot and reset;
- no bypass flag.

The limiter follows the complete user chain and is the last audio-processing stage. Detector taps are taken at the output-chain end. The raw peak, excursion-weighted peak, and RMS detectors independently request attenuation. In linear amplitude terms:

- g_peak = min(1, peak_ceiling / peak_detected)
- g_exc = min(1, excursion_limit / excursion_detected)
- g_rms = min(1, rms_limit / rms_detected)
- g_target = min(g_peak, g_exc, g_rms)

Convert each dBFS threshold to linear amplitude before comparison. A zero detector magnitude requests unity gain. Applied gain is constrained to 0..1; no detector may boost. Lookahead and attack are fixed at 1 ms. Release uses release_ms. Every gain change reuses the existing T18 ramp executor and its established per-sample bound/rules; no second ramp implementation is permitted. The final output clamp must not exceed peak_ceiling_dbfs.

Missing, malformed, or out-of-range protection leaves the affected output muted. The system starts muted and may unmute only after every configured output has a valid protection object and the complete candidate has passed validation and safe installation. There is no bypass or permissive default.

## Validator rules and fail-closed behavior

The schema is normative. Firmware checks structure, finite numeric values, enumerations, and declared bounds only; it must not perform driver, amplifier, PSU, or response-curve calculations.

| Reject condition | Required behavior |
|---|---|
| Invalid JSON, non-object root, duplicate object keys, trailing data, parse/resource limit exceeded | Reject candidate; remain muted or retain the current validated configuration |
| Unknown root/output/protection/filter property | Reject; includes T/S names, driver curves, amp/PSU fields, volts, watts, and bypass |
| Wrong/missing version or sample rate | Reject |
| Missing, empty, oversized, unknown, or duplicate input declaration | Reject |
| Missing/empty/oversized outputs; invalid ID; duplicate output ID or TDM slot | Reject |
| Missing/malformed input mix or chain; undeclared/duplicate input channel; non-finite, zero, or out-of-range mix weight | Reject |
| Missing per-output protection object or required field | Reject; output stays muted |
| Threshold outside -60..0 dBFS | Reject |
| RMS tau outside 100..60000 ms or release outside 20..5000 ms | Reject |
| Excursion weighting is not an array or contains more than four filters | Reject |
| Unsupported filter type; missing/extra filter key; non-finite or out-of-range f_hz/q/gain_db; gain_db present on hp/lp or absent on peq/shelf | Reject |
| Attempt to configure lookahead, attack, DC blocker, mute, or bypass | Reject as unknown/forbidden property |
| Candidate cannot be fully validated or safely installed at DSP ownership boundary | Do not publish; retain previous validated config or remain muted |

The JSON Schema defines the per-output field names, filter enum, and numeric ranges. Cross-field checks such as output ID/slot uniqueness, declared input references, resource limits, and safe installation are semantic structure/bounds checks. Firmware does not judge the acoustic or thermal correctness of designer-selected filter parameters.

### A/B slot fallback

A/B slots store complete device JSON candidates. Slot header/version/length/CRC checks are storage-integrity checks, not driver-model validation.

1. At boot, keep amplifier mute asserted. Check both slot headers, payload lengths, and CRCs independently.
2. Parse and validate each integrity-valid payload against the current schema and semantic structure/bounds rules.
3. If the selected active slot is fully valid, load it.
4. If the selected slot is invalid and the other slot is fully valid, fall back to the other slot.
5. If neither is fully valid, or the active-slot selector is invalid, remain muted and report a configuration fault.
6. On update, validate before writing the inactive slot. Write, read back, and verify integrity; only then publish the active selector at a safe boundary. Failed candidate/write/readback must not damage the last known-good slot.

Fallback never relaxes schema, bounds, or mute requirements.

## Designer mapping appendix (informative; not firmware rules)

The designer tool owns physical modelling and folds results into the abstract dBFS thresholds and detector weighting. These equations are illustrative mappings, not schema fields. Use consistent RMS or peak conventions throughout.

### Amplifier clipping

Let V_DAC_FS be measured DAC full-scale voltage, G_A the fixed voltage gain from DAC to amplifier output, and V_CLIP the amplifier clipping voltage on the same convention:

- V_AMP_FS = G_A × V_DAC_FS
- L_CLIP,dBFS = 20 log10(V_CLIP / V_AMP_FS) - M_CLIP

The designer selects a conservative peak ceiling from this result. Convert sine peak and RMS specifications before comparing them.

### Driver thermal limit

For a resistive approximation using continuous thermal rating P_E and nominal resistance R_E:

- V_TH = sqrt(P_E × R_E)
- L_TH,dBFS = 20 log10(V_TH / V_DRV_FS) - M_TH

V_DRV_FS is the predicted driver-terminal voltage for a 0 dBFS output-chain signal. The designer chooses rms_limit_dbfs no higher than the most restrictive thermal/amplifier limit and chooses rms_tau_ms from the applicable thermal model and programme assumptions. The simple P_E × R_E approximation is not a substitute for impedance, cooling, duty-cycle, enclosure, or manufacturer guidance.

### Excursion limit and weighting

Let X_FS(f) be predicted cone displacement versus frequency for a 0 dBFS output-chain signal, including enclosure and port contribution. A conservative scalar bound is:

- L_X,dBFS = min_f [20 log10(X_MAX / X_FS(f))] - M_X

The designer chooses excursion_weighting to track the frequency dependence of displacement and exports excursion_limit_dbfs against that weighting's normalization. For a ported enclosure, include excursion maxima around and below port tuning. Do not assume a sealed-box or driver-only curve is sufficient. The weighting cascade is a designer-selected approximation, not a firmware driver model.

Margins, normalization, filter fit quality, tolerances, amplifier gain, clipping, and supply behavior require sourced inputs and human review. Firmware cannot validate those assumptions.

## Retired comparator and SciPy oracle

The production C response comparator/API and its firmware gate are retired. Former Gate 3 (C comparator API) and Gate 5 (sourced driver data) are retired as firmware gates and move outside firmware scope.

Keep the independent NumPy/SciPy oracle. Re-scope it as designer-side per-output margin reporting and as a numerical reference for C biquad and limiter math tests. It is not linked into firmware and is not a runtime validator. The current implementation covers static chain response against supplied curves; limiter-specific oracle vectors/tests are future work and must not be described as already implemented. The oracle does not infer driver models.

## Human gates and verification

Remaining human gates:
1. Approve numerical conventions: dBFS reference, peak/RMS detector definitions, filter coefficient convention, excursion-weight normalization, equality and rounding.
2. Select the exact ESP32-S3 module and approve internal SRAM/PSRAM/flash and DSP-state memory budget.
3. Approve ADR 0014 and the schema contract.

No firmware code was changed. No hardware or bench verification was performed. This proposal is not evidence of safe limits or a measured output ceiling.
