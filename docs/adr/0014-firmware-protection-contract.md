# ADR 0014: Firmware protection contract — designer-derived limits only

- Status: Proposed (amended 2026-11-22: contract v1.1); requires maintainer and hardware-safety review before implementation or unmuting hardware.
- Date: 2026-11-22
- Supersedes: the protection/comparator decisions in ADRs 0008, 0010, and 0013.
- Applies to: device JSON, firmware validation, DSP protection execution, and the Step-4 bench slice.

## Context

The speaker designer owns driver, amplifier, and supply modelling. The firmware consumes only a bounded configuration containing the exported filter parameters and abstract protection limits it must validate and execute. Driver T/S data, amplifier characteristics, PSU models, and VituixCAD project data remain designer-side inputs and must not become firmware configuration or runtime state.

## Decision

### 1. Designer/firmware trust boundary

The designer tool may consume driver parameters such as Fs, Qts, Vas, Re, Le, Xmax, Pe, Sd and related fields, amplifier gain and clipping data, rail data, PSU capability, and VituixCAD results.

None of those model or hardware fields may appear in firmware structures or device JSON. The designer tool folds their consequences into exported detector weighting filters, thresholds, time constants, release, and ceiling. Firmware validates structure, finite numeric values, declared ranges, filter designability/stability, and cross-field relationships. It does not calculate driver excursion, thermal capability, amplifier clipping voltage, rail sag, watts, or driver-derived limits.

The device JSON is a closed contract. The top-level protection object and every nested protection object use `additionalProperties: false`. Unknown fields—including T/S parameters, amplifier fields, PSU fields, and future model extensions—are rejected. A rejected candidate leaves outputs muted.

### 2. Protection block schema

The normative device JSON shape is `configs/schema.json`. Each protection output is joined to exactly one configured output by `id`; IDs must be unique and the sets must match exactly.

Each output protection record contains:

- `weighting`: a detector-only `filters[]` list, at most four parametric filters. It applies to the peak detector path only. An empty list means flat peak weighting.
- `peak_threshold_dbfs`: weighted peak detector threshold in dBFS.
- `hold_ms`: peak-hold applied to the weighted peak detector before the gain computer. The designer sets it to at least half the period of the lowest in-band frequency so the gain does not follow the waveform within a cycle.
- `thermal`: one or two unweighted RMS detector stages, each `{rms_threshold_dbfs, rms_tau_ms}`. Stage time constants must be strictly increasing, so stage 1 models the fast (voice-coil) pole and stage 2 the slow (magnet/frame) pole. The gain reduction is the larger reduction of the stages.
- `release_ms`: attenuation release time of the peak path. Thermal gain follows its RMS detectors directly and has no separate release.
- `ceiling_dbfs`: final per-output sample ceiling in dBFS.

**RMS reference.** `rms_threshold_dbfs` is the RMS of the normalised output samples with full scale = 1.0. A full-scale sine therefore reads -3.01 dBFS, and a full-scale square wave reads 0 dBFS. The designer must use the same reference when converting a power or voltage limit.

**Links.** The optional top-level `links` is a list of groups of output IDs (2 to 8 IDs per group, at most 4 groups). Every ID must name a configured output and may appear in at most one group, once. Within a group the applied gain is the minimum (largest reduction) across the members, so linking can only attenuate more than an unlinked output would. Linking is for image stability of matched pairs; do not link different ways of one speaker.

**Resource bound.** The total filter count across `pre`, output chains and detector weighting is at most 96 (`E_RESOURCE`), with at most 16 filters per list. The 96 cap is a planning value pending bench CPU measurement.

The weighting filter entries use the same parametric filter representation as output chains: `type`, `f_hz`, `q`, and `gain_db`. The permitted filter types and numeric bounds are defined by the schema. Cross-field validation requires every threshold and the ceiling to be at most 0 dBFS (schema bounds), unique links, and strictly increasing thermal time constants. All numeric bounds, including the RMS integration and release ranges, filter frequency/Q/gain limits, and filter-count limits, are normative in `configs/schema.json`. That schema is the single source of truth; this ADR intentionally does not duplicate those values. The protection record does not contain `lookahead_ms`, `attack_ms`, per-detector attack fields, `subsonic_hp`, or a protection `crc32`. Since there is no separate `subsonic_hp` field, infrasonic attenuation is entirely the speaker designer's responsibility through the exported crossover/filter design and peak-detector weighting; firmware must not imply that an independent infrasonic protection stage exists.

The output chain is designer-authored as a bounded `filters[]` list. An empty output `filters[]` is valid and means no user-authored filtering; it is the speaker designer's responsibility to ensure that a full-range signal is not accidentally routed to a driver (for example, a tweeter) that requires a crossover. Firmware expands the parametric filters into biquad sections once during configuration load; the real-time audio path does not design filters.

The A/B configuration slot CRC (T20) is the integrity mechanism for stored configuration bytes. Do not add a second protection-object CRC or require JSON canonicalization for protection integrity.

The worked example contains synthetic limits and coefficients only. They are not sourced, approved, or safe for any particular driver, amplifier, PSU, or speaker. Do not flash the example to connected hardware.

### 3. Signal path and limiter contract

For every output, the mandatory order is:

1. input mix;
2. the ordered user-authored output filter stack and output gain/polarity/delay;
3. protection detection in parallel with the signal path;
4. lookahead attenuation and final ceiling clamp;
5. system ramp/mute and output.

The protection stage receives the signal after the crossover has already band-limited it. The RMS detectors therefore operate on that unweighted signal. Do not apply detector weighting to the thermal RMS estimate. The configured weighting filters are used only by the peak detector.

The limiter uses a fixed 1 ms lookahead window (48 samples at 48 kHz). Its algorithm class is a lookahead peak limiter: delay the audio path by 48 samples; compute the required attenuation gain from the undelayed detector signal; take the minimum required gain over the corresponding 48-sample lookahead window; then smooth the applied gain so it reaches that target no later than the end of the 1 ms window. The exact gain-envelope interpolation must be deterministic and covered by host vectors. It does not reuse the T18 S7 ramp as its attack mechanism. S7 is reserved for mute, configuration swap, and user gain transitions. Required vectors cover an isolated single-sample peak, a short burst, and a sustained sine. The limiter's final hard clamp at `ceiling_dbfs` is the last-resort enforcer of the sample invariant:

`|y[n]| <= 10^(ceiling_dbfs/20)`

The limiter gain is attenuation-only: it must never exceed unity or increase signal level. The 1 ms lookahead and attack are a fixed implementation contract, not configurable JSON fields. Release remains configurable. The final hard clamp must remain effective even if the detector or lookahead attenuation path is late or otherwise insufficient. This clamp enforces the digital output-sample ceiling and can prevent samples exceeding the configured full-scale ceiling; it is not an excursion limiter and does not guarantee that a loudspeaker remains within Xmax. Detector weighting can introduce frequency-dependent group delay; near a low-frequency corner that delay can exceed 1 ms (approximately 1/(2πf₀), or about 5.3 ms at 30 Hz). A weighted peak detector may therefore react after the corresponding transient has passed the lookahead point. Excursion overshoot must be handled in the designer's model and margin, not claimed as guaranteed by the firmware clamp. The designer should include an explicit excursion margin M_x (a 1–2 dB starting suggestion, not a validated universal value) and verify it against driver-specific burst behaviour. Add a burst vector that measures peak excursion-model overshoot relative to the intended limit.

The top-level `protection` block may be omitted from a structurally valid configuration intended for staging or provisioning. Such a configuration is unprovisioned, not enabled: firmware must leave every output muted until a valid protection record covers every configured output. Boot, reset, invalid/missing protection, failed validation, or incomplete output coverage leaves hardware muted. Output may be enabled only after the complete configuration validates and the DSP owner installs the corresponding runtime state at a safe boundary.

### 4. Static response oracle and retired production comparator

Retire the production C comparator and its API. There is no firmware C comparator contract, public comparator header, or comparator task in this repository. Driver-model and T/S-derived protection gates belong to the designer tool.

Keep the independent SciPy chain-response oracle and its deterministic vectors for checking parametric filter response. The oracle is not a firmware runtime dependency and does not establish physical loudspeaker safety. Its inputs must not require driver T/S, amplifier, or PSU model records.

No phase-cancellation credit may be used in conservative multi-input margin reporting if such designer-side reporting is retained. Out-of-range or non-finite oracle inputs are errors; the oracle does not manufacture driver or amplifier facts. Any designer-side margin is a report to the speaker designer, not a firmware decision that proves a physical loudspeaker safe.

### 5. Hardware safety is independent

Firmware limiting is one layer, not the sole safety mechanism. The hardware design must provide:
- amplifier mute asserted during boot/reset and until validated enable;
- DC blocking appropriate to the amplifier and output topology;
- a correctly selected speaker fuse or PTC on the subwoofer output;
- a safe dummy load or sacrificial driver for initial bench work.

A software ceiling does not protect against firmware faults, DAC/amp DC faults, wiring faults, or a failed mute path. Do not connect valuable speakers during first bring-up.

## Consequences and implementation sequence

- Migrate the schema, example, and guardrail validator together to the frozen parametric-filter contract.
- Remove the obsolete protection comparator, curve-knot oracle, and their task-card authority. Keep the SciPy chain-response oracle and vectors.
- Generate task cards from the schema rather than maintain repeated hand-edited contract fragments.
- The firmware implementation sequence is: configuration validator and filter expansion, peak/RMS detector, fixed-lookahead limiter with final clamp, then mandatory output-chain wiring. Each component requires host tests against the relevant numerical oracle.
- The 2.1 hardware bench slice follows only after the contract, implementation tests, hardware mute/DC/fuse provisions, and selected target memory budget are reviewed.

## Open decisions requiring explicit approval

- **Lookahead length (safety behaviour).** The fixed 1 ms value above is unchanged pending this decision. A host screening model (30 Hz full-scale burst, linear sealed-box excursion weighting, instantaneous-abs detector; not a loudspeaker simulation) showed excursion overshoot of about 3.9 dB at 1 ms lookahead/attack, 2.0 dB at 6 ms, and steady-state gain-modulation distortion of 2.3-2.8 % that fell to 0 % with a 17 ms peak-hold. The recommendation is a fixed lookahead near 6 ms with attack equal to the lookahead; confirm by the bench overshoot measurement before freezing.
- **Where the protection block is stored and who may change it.** AGENTS.md S2-S5 describe protection limits in NVS written only by a serial provisioning tool and never overridable over the network. This ADR places the protection block in the uploaded configuration. One of the two must change. Recommended: keep one document, and accept a change to the protection block only while a serial or physical provisioning mode is active.

- Exact ESP32-S3 module and verified memory budget.
- Final numeric protection limits and coefficients for each real output, supplied by the designer tool from sourced data and reviewed by the owner.
- Hardware mute GPIO, DC-blocking implementation, fuse/PTC selection, and bench measurements.

## Verification status

This ADR and its accompanying schema/example define a proposed contract. They do not claim that firmware validation, limiter DSP, hardware mute, DC blocking, fusing, or bench performance has been implemented or verified. The synthetic example is not a hardware protection profile.
