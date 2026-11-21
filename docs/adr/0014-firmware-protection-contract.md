# ADR 0014: Firmware protection contract — designer-derived limits only

- Status: Proposed; requires maintainer and hardware-safety review before implementation or unmuting hardware.
- Date: 2026-11-22
- Supersedes: the protection/comparator decisions in ADRs 0008, 0010, and 0013.
- Applies to: device JSON, firmware validation, DSP protection execution, and the Step-4 bench slice.

## Context

The previous protection design placed driver, amplifier, and supply models in a separately provisioned firmware block and proposed a production C response comparator. That boundary is too broad for the firmware product. It duplicates speaker-design knowledge in the device and makes driver-model fields part of the long-lived firmware contract.

The speaker designer already has the appropriate domain tools and source data: VituixCAD response, driver T/S parameters, amplifier gain and clipping data, and PSU/rail information. Firmware needs only a bounded, self-contained protection block containing the calculated coefficients and abstract limits it must validate and execute.

## Decision

### 1. Designer/firmware trust boundary

All driver and hardware modelling is designer-side. The designer tool may consume driver parameters such as Fs, Qts, Vas, Re, Le, Xmax, Pe, Sd and related fields, as well as amplifier gain, clip voltage, rail data, PSU capability, and VituixCAD results.

**None of those model or hardware fields may appear in firmware structures or device JSON.** They are not firmware configuration, runtime state, NVS records, validator inputs, or device API fields. The designer tool folds their consequences into the exported weighting biquads, thresholds, time constants, ceiling, optional subsonic high-pass, and optional normalized shared-supply budget.

The device JSON is a closed contract. The top-level `protection` object and every nested protection object use `additionalProperties: false`. Unknown fields—including T/S parameters, amplifier fields, PSU fields, and future model extensions—are rejected. A protection block containing `Qts`, `Vas`, `Xmax`, `Re`, or similar data is invalid. The complete candidate is rejected and outputs remain muted.

Firmware validates structure, numeric finiteness, declared ranges, biquad stability, coefficient/sample-rate compatibility, cross-field relationships, and CRC. It does not calculate driver excursion, thermal capability, amplifier clipping voltage, rail sag, watts, or driver-derived limits. Firmware validation is not a substitute for designer-side engineering judgement or hardware protection.

### 2. Protection block schema

The normative device JSON shape is `configs/schema.json`. Each protection output is joined to exactly one configured output by `id`; IDs must be unique and the sets must match exactly. The block contains:

- `weighting_biquads`: zero or more stable biquad coefficient pairs applied to the detector signal. An empty array means flat detector weighting.
- `peak_threshold_dbfs`: weighted peak detector threshold in dBFS.
- `lookahead_ms`: lookahead duration, bounded to 1–2 ms in v1 (48–96 samples at 48 kHz).
- `rms_threshold_dbfs`: weighted RMS detector threshold in dBFS.
- `rms_tau_ms`: RMS detector integration time constant.
- `attack_ms` and `release_ms`: attenuation-control response times.
- `ceiling_dbfs`: final per-output ceiling in dBFS.
- `subsonic_hp` (optional): one stable biquad applied in the protection signal path before the final limiter gain stage.
- `shared_supply_budget` (optional, protection-block level): `max_normalized_sum_rms_power`, the maximum sum of per-output RMS-squared values normalized to each output's full-scale reference. It is dimensionless, not watts. If absent, v1 uses per-output protection only.

Biquad coefficients use numerator `b: [b0,b1,b2]` and denominator `a: [a1,a2]`, representing `(b0+b1*z^-1+b2*z^-2)/(1+a1*z^-1+a2*z^-2)`. The device sample rate is fixed at 48 kHz. Coefficients must be finite and stable under the firmware's existing biquad stability contract.

The protection object carries `crc32`: eight hexadecimal digits containing CRC-32/ISO-HDLC over the protection object with the `crc32` member omitted, serialized using RFC 8785 JSON Canonicalization Scheme (UTF-8). Any mismatch rejects the complete candidate. The CRC detects accidental corruption; it is not authentication or a trust mechanism.

The worked 2.1 example in `configs/example_2way_sub.json` demonstrates two flat-weighted main outputs and a subwoofer with its detector weighting expressed as two biquads, plus an optional subsonic HP. Its numeric limits and coefficients are synthetic schema examples only—not sourced, approved, or safe for any particular driver, amplifier, PSU, or speaker. Do not flash the example to connected hardware.

### 3. Signal-path and limiter contract

For every output, the mandatory order is:

1. input mix and optional input pre-chain;
2. user-authored DSP chain, including user PEQ and gain;
3. optional protection subsonic high-pass;
4. weighted peak and RMS detection in parallel with the audio path;
5. attenuation-only limiter gain and final ceiling enforcement;
6. system ramp/mute and output.

The limiter is the final signal-processing stage after user PEQ and gain. Detection uses the configured weighting biquads and may request attenuation only. The limiter gain must never exceed unity and must never increase a signal level. The final sample invariant is `abs(y[n]) <= 10^(ceiling_dbfs/20)` for every output sample.

Lookahead is 1–2 ms at 48 kHz, i.e. 48–96 samples. The limiter's attenuation changes must ramp smoothly using the existing T18 ramp executor; do not introduce a second ramp implementation. Preserve T18's per-sample executor bound of `0.0011f`. The attack/release controls shape detector/control behavior; they do not authorize gain above unity or bypass the ceiling.

A protection block is mandatory for activation. Boot, reset, invalid/missing block, CRC failure, failed validation, or incomplete output coverage leaves the hardware muted. Output may be enabled only after the complete configuration and protection block validate and the DSP owner has installed the corresponding runtime state at a safe boundary.

### 4. Static response oracle and retired production comparator

Retire the production C comparator and its API (the former Gate 3). There is no firmware C comparator contract, no public comparator header, and no comparator task in this repository. Retire the former T/S-derived protection gates, including Gate 5 (sourced driver data), from firmware scope. They belong to designer-tool scope.

Keep the independent SciPy oracle as:
- a designer-side margin report that evaluates exported protection weighting and the user signal chain against designer-provided response/limit data; and
- a test oracle for C biquad and limiter numerical behavior, including coefficient response, detector weighting, attenuation, lookahead, and ceiling vectors.

The oracle is not a firmware runtime dependency and is not a production C comparator. Its inputs must be limited to abstract exported coefficients/limits and synthetic or designer-side reference vectors. Driver T/S, amplifier, and PSU model records must not be required by firmware tests or embedded in firmware oracle fixtures.

No phase-cancellation credit may be used in conservative multi-input margin reporting. Out-of-range or non-finite oracle inputs are errors; the oracle does not manufacture driver or amplifier facts. Any designer-side margin is a report to the speaker designer, not a firmware decision that proves a physical loudspeaker safe.

### 5. Hardware safety is independent

Firmware limiting is one layer, not the sole safety mechanism. The hardware design must provide:
- amplifier mute asserted during boot/reset and until validated enable;
- DC blocking appropriate to the amplifier and output topology;
- a correctly selected speaker fuse or PTC on the subwoofer output;
- a safe dummy load or sacrificial driver for initial bench work.

A software ceiling does not protect against firmware faults, DAC/amp DC faults, wiring faults, or a failed mute path. Do not connect valuable speakers during first bring-up.

## Consequences and implementation sequence

- Update the device schema and worked example to this contract before implementing firmware.
- Remove or relocate driver-model files, T/S fields, amplifier/PSU data, and T/S-derived oracle inputs during the Step-2 repository cleanup. They belong in the designer-tool repository, not firmware.
- Reconcile ADR 0008, 0010, and 0013 and all requirements/task-card text against this ADR; no parallel legacy protection authority may remain.
- The Step-2 rework must generate task cards from this contract rather than maintain repeated hand-edited contract fragments.
- The next firmware implementation order is: protection validator (including CRC and stable biquads), weighted peak/RMS detector, lookahead limiter, then mandatory output-chain wiring. Each component requires unit tests against the independent SciPy oracle.
- The 2.1 hardware bench slice follows only after the contract, implementation tests, hardware mute/DC/fuse provisions, and selected target memory budget are reviewed.

## Open decisions requiring explicit approval

- Whether the optional shared-supply budget is included in the first 2.1 deployment; per-output limits remain sufficient when it is omitted.
- Exact ESP32-S3 module and verified memory budget.
- Final numeric protection limits and coefficients for each real output, supplied by the designer tool from sourced data and reviewed by the owner.
- Hardware mute GPIO, DC-blocking implementation, fuse/PTC selection, and bench measurements.

## Verification status

This ADR and its accompanying schema/example define a proposed contract. They do not claim that firmware validation, limiter DSP, hardware mute, DC blocking, fusing, or bench performance has been implemented or verified. The synthetic example is not a hardware protection profile.
