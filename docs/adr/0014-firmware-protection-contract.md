# ADR 0014: Firmware protection contract — designer-derived limits only

- Status: Proposed (amended 2026-11-22: contract v1.1; limiter timing and protection mutation policy decided by the maintainer 2026-11-22; admin window proposed by the maintainer 2026-11-23); requires CODEOWNERS and hardware-safety review before implementation or unmuting hardware.
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

The limiter uses a fixed lookahead L = 12 ms (576 samples at 48 kHz) and a fixed attack A = 2 ms (96 samples). Both are compile-time constants, `XOVER_LIMITER_LOOKAHEAD_SAMPLES` and `XOVER_LIMITER_ATTACK_SAMPLES`, defined once in one protection-component header; every test derives its expectations from them, so a bench result changes them in one place. They are not JSON fields. Release remains configurable.

Reference algorithm (normative for the host vectors): delay the audio path by L samples; from the undelayed detector signal compute the required gain `r[n] = min(1, threshold / held weighted peak)`; take the minimum of `r` over the last A samples and smooth it with a moving average of A samples; let the gain rise only through a one-pole release with the configured `release_ms`; output `y[n] = x[n-L] * g[n]`. The gain never exceeds 1. The T18 S7 ramp is not the attack mechanism; S7 is reserved for mute, configuration swap and user gain transitions. Required vectors: an isolated single-sample peak, a short burst, a sustained sine, and the burst-overshoot vector below.

**Why L is not equal to A.** Audio that arrives at detection time m is fully attenuated at output only if L >= A + tau, where tau is the lag between the audio that excites the excursion and the weighted detector peak. If attack equals lookahead, the gain reaches its target only for audio arriving at detection time, after the audio that caused the peak has already passed. The screening below shows this: with A = L the overshoot falls roughly as 1/L and does not reach zero at 16 ms, while with A = 2 ms it drops sharply once L reaches A + tau.

**Design rule.** tau is the low-frequency group delay of the designer's weighting. For a second-order sealed-box weighting it is `tau0 = 1/(Q*w_c)`: 7.5 ms at Fc 30 Hz and Q 0.707, 4.5 ms at 50 Hz, 2.8 ms at 80 Hz. (The earlier draft used `1/(2*pi*f0)` = 5.3 ms at 30 Hz, which is the first-order figure and understates a second-order weighting.) The fixed L - A = 10 ms therefore covers sealed weightings down to Fc about 30 Hz. A weighting with larger group delay (a lower Fc, or a vented or higher-order alignment) is outside the contract. A load-time check that rejects a weighting whose group delay at 20 Hz exceeds 10 ms is planned as a validator task; it is not yet in `schema.json` or `check.py`.

**Screening evidence** (`python tools/lookahead_screen.py`; HOST-SCREENING, not a gate). Worst excursion overshoot in dB above the limit over bursts at 0.6, 1.0 and 1.5 x Fc, linear sealed-box weighting with Q 0.707, 17 ms hold, burst 6 dB above the limit:

| Fc | attack | L=2 | L=4 | L=6 | L=8 | L=10 | L=12 | L=16 ms |
|---|---|---|---|---|---|---|---|---|
| 30 Hz | A = L | 4.81 | 4.11 | 3.37 | 2.73 | 2.27 | 1.94 | 1.51 |
| 30 Hz | A = 2 ms | 4.81 | 3.34 | 2.07 | 1.25 | 0.70 | 0.39 | 0.19 |
| 50 Hz | A = L | 4.35 | 3.13 | 2.26 | 1.75 | 1.42 | 1.19 | 0.91 |
| 50 Hz | A = 2 ms | 4.35 | 1.97 | 0.63 | 0.15 | 0.02 | 0.01 | 0.01 |
| 80 Hz | A = L | 3.60 | 2.13 | 1.50 | 1.16 | 0.94 | 0.78 | 0.58 |
| 80 Hz | A = 2 ms | 3.60 | 1.00 | 0.25 | 0.16 | 0.07 | 0.00 | 0.00 |

With L = 12 ms and A = 2 ms the worst case in the table is 0.39 dB (Fc 30 Hz) and at most 0.01 dB for Fc >= 50 Hz. The previous coupled form (A = L) at 2 ms gives 3.6 to 4.8 dB in the same model, and at 12 ms still 0.8 to 1.9 dB. The 17 ms hold, taken as half the period of the lowest in-band frequency (30 Hz), was not re-evaluated for gain-modulation distortion with A = 2 ms; that is a bench item.

**Cost.** 576 float32 samples are 2.3 KB per output, 18.4 KB for 8 outputs, in internal SRAM (R3). The audio latency is 12 ms, applied to every output, so the relative alignment between outputs is unchanged.

**Caveat.** The model is a linear sealed box with a second-order weighting and an instantaneous-peak detector. A vented subwoofer below its tuning excurses more than the model predicts, and its weighting has larger group delay. The bench overshoot measurement is the real gate; until it is recorded the values are UNVERIFIED. The final hard clamp must remain effective even if the detector or lookahead attenuation path is late or otherwise insufficient. This clamp enforces the digital output-sample ceiling and can prevent samples exceeding the configured full-scale ceiling; it is not an excursion limiter and does not guarantee that a loudspeaker remains within Xmax. Detector weighting introduces frequency-dependent group delay (see the design rule above), so a weighted peak detector reacts after the excursion-causing audio. The residual overshoot must be handled in the designer's model and margin, not claimed as guaranteed by the firmware clamp. The designer should include an explicit excursion margin M_x (a 1–2 dB starting suggestion, not a validated universal value) and verify it against driver-specific burst behaviour. Add a burst vector that measures peak excursion-model overshoot relative to the intended limit.

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

### 6. Protection mutation policy

The device JSON stays one document and contains the protection block. The committed protection block is the one in the active validated configuration slot (the A/B slot with CRC, T20). There is no separate NVS limit store.

- **P1 Network upload is locked to the committed protection block.** Outside an admin window (P3), an upload over any network path must carry a protection block. After parsing and validation, firmware compares it numerically with the committed block: per output id, the weighting filters in order (`type`, `f_hz`, `q`, `gain_db`), `peak_threshold_dbfs`, `hold_ms`, the thermal stages in order, `release_ms` and `ceiling_dbfs`. Any difference, or no committed block (an unprovisioned device), rejects the candidate with `E_PROTECT_LOCKED`. The running configuration and the mute state stay unchanged. The comparison is exact equality of the parsed values; there is no tolerance, no canonical text form and no extra CRC.
- **P2 What the network may change.** `inputs`, `pre`, `outputs` (chains, gain, polarity, delay, slot; the output id set must still equal the protection id set) and `links`. Links only take the minimum gain across a group, so they can only add attenuation and are not locked.
- **P3 Provisioning mode (admin window).** The protection block changes only inside an admin window. The window is armed only by a local physical action: holding the admin button for 5 s continuously (releasing early aborts), or a command on the USB serial port (both need physical access to the device; the serial command exists for first provisioning and bench work). No HTTP endpoint, JSON field, timer, scheduled task or other API can arm it. A hold that begins before boot has finished is ignored (GPIO0 held at reset selects the ROM download mode, which is unrelated). Proposed by the maintainer 2026-11-23; the rules below harden it.
  - **Entry.** Arming ramps the outputs down (S7) and mutes them, and they stay muted for the whole window. The admin LED shows a distinct pattern while the window is open. The hold time and the window length are compile-time constants (`XOVER_ADMIN_HOLD_MS` = 5000, `XOVER_ADMIN_WINDOW_S` = 300), each defined once.
  - **Closing.** The window closes on the first successful commit (single use), on timeout, on reboot or reset, on a second button hold, and after 5 failed authentication attempts. Closing without a commit leaves the committed configuration unchanged and returns to the normal validated unmute path.
  - **Access control.** While the window is closed, every admin route answers exactly like an unknown path (same status and body) and the admin page is not served; this hides the feature and is not the security control. While it is open, the server checks every admin request for: the window is armed; the upload token (X2, constant-time compare); a `Host` header equal to the device's own address or name and, if an `Origin` header is present, an equal `Origin` (this blocks cross-site and DNS-rebinding requests); `Content-Type: application/json`; and the X3 size and depth bounds. Failed attempts are limited to one per second and are counted.
  - **Warning.** The admin page warns that changing protection can damage speakers and amplifiers and needs an explicit acknowledgement before its upload control is enabled. This is a usability measure, not a security control.
  - **Commit.** The candidate goes through the full validation (the P1 comparison is the only check waived, and only for the protection block) and an atomic A/B slot commit with CRC (T20). The previous slot is kept so a later boot can fall back to it. Unmuting happens only through the normal validated path after the commit or after the window closes.
  - **Audit.** Every arm, disarm (with the reason), authentication failure and commit (old and new slot CRC) is written to the serial log and to a bounded persistent event log. The token and the protection values are never logged.
  - **Transport.** The device is meant for a trusted local Wi-Fi network (WPA2 or WPA3) and is never exposed to the internet (X5). Over plain HTTP, anyone on that network can read the token and the uploaded block during the window. The admin routes should use HTTPS from the IDF HTTPS server, which is not a new dependency; this is an open decision below.
  - **What this does and does not stop.** A person on the Wi-Fi network without the token and without physical access cannot change protection. A person with physical access can arm the window, and a person who also has the token can change protection. Neither case is stopped by firmware.
- **P4 Fail closed.** Boot, reset, a missing or invalid committed block, a failed validation or incomplete output coverage leaves every output muted.
- **P5 Designer requirement: validity out of band.** The output chain is network-editable and the protection detects after it, so a bass boost or a changed crossover is seen by the detectors. That holds only if the exported weighting and thresholds are conservative for any in-spec chain edit, that is over the whole band from 20 Hz to Nyquist, not only for the signal the original crossover passes. The RMS thermal detectors are unweighted on the post-chain signal, so extra out-of-band energy can only raise them. The peak weighting must read the driver's excursion or voltage sensitivity across the whole band, for example a tweeter weighting that reads low-frequency content as large.
- **Residual risk (needs maintainer acknowledgement).** If a user deletes a tweeter's high-pass filter over the network, protection holds only as far as the designer's out-of-band weighting does; the digital ceiling does not protect a tweeter. An option not adopted here is to let the designer mark outputs whose chains are locked like the protection block.
- AGENTS.md S2 to S5 are rewritten to match this policy.

## Consequences and implementation sequence

- Migrate the schema, example, and guardrail validator together to the frozen parametric-filter contract.
- Remove the obsolete protection comparator, curve-knot oracle, and their task-card authority. Keep the SciPy chain-response oracle and vectors.
- Generate task cards from the schema rather than maintain repeated hand-edited contract fragments.
- The firmware implementation sequence is: configuration validator and filter expansion, peak/RMS detector, fixed-lookahead limiter with final clamp, then mandatory output-chain wiring. Each component requires host tests against the relevant numerical oracle.
- The 2.1 hardware bench slice follows only after the contract, implementation tests, hardware mute/DC/fuse provisions, and selected target memory budget are reviewed.

## Open decisions requiring explicit approval

- **Decided 2026-11-22 by the maintainer, pending CODEOWNERS and hardware-safety review:** limiter lookahead 12 ms with attack 2 ms (section 3), and the protection mutation policy (section 6).
- **Proposed by the maintainer 2026-11-23, hardened in section 6 P3, pending CODEOWNERS and hardware-safety review:** the admin window (physical 5 s hold or USB serial command, single use, 5-minute timeout, token required).
- Still open for T07b: HTTPS for the admin routes, the LED patterns, whether the serial arm command stays in production builds, and the size of the persistent event log.
- A load-time group-delay check for the weighting (validator task).
- Exact ESP32-S3 module and verified memory budget.
- Final numeric protection limits and coefficients for each real output, supplied by the designer tool from sourced data and reviewed by the owner.
- Hardware mute GPIO, DC-blocking implementation, fuse/PTC selection, and bench measurements.

## Verification status

This ADR and its accompanying schema/example define a proposed contract. They do not claim that firmware validation, limiter DSP, hardware mute, DC blocking, fusing, or bench performance has been implemented or verified. The synthetic example is not a hardware protection profile.

Limiter lookahead and attack values are HOST-SCREENING only (`tools/lookahead_screen.py`, output pasted in section 3); no bench overshoot or modulation-distortion measurement exists.
