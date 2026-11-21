# Requirements

## Product requirements

- Initial product: a 2.1 active crossover with stereo input and six output
  channels (left woofer, left tweeter, right woofer, right tweeter, sub A,
  sub B). The architecture must extend to 5.1 without a general-purpose
  graph editor.
- Target: ESP32-S3 with I2S TDM DAC output. Initial sample rate is 48 kHz.
- DSP executes on the ESP32-S3. The external speaker-design tool calculates
  crossover and equalization values; firmware validates and executes them.
- Configuration is uploaded as JSON over the device network interface.
- A speaker designer defines optional shared input pre-chains, per-output
  input mixes, and ordered per-output DSP chains.
- Each output receives a compiler-appended mandatory protection pipeline,
  system-wide power-budget enforcement, final clamp, ramp, and mute.
- The initial DSP block catalog reuses tested crossover, PEQ, shelf, all-pass,
  notch, biquad, gain, delay, polarity, and mix primitives where appropriate.
  FIR remains deferred.
- DSP processing runs on core 1; networking, decoding, configuration,
  validation, and chain construction run on core 0.

## Configuration rules

- Top-level configuration version key is `v`; sample-rate key is `fs`.
  Both are mandatory. The exact units and allowed values are defined in the
  schema.
- Unit-bearing key names are required for physical quantities.
- Every output must declare its input mix, ordered user chain, and mandatory
  protection configuration.
- Unknown keys and missing or misspelled mandatory protection keys reject.
- Uploaded configuration cannot change the separately provisioned protected
  block or weaken any device protection limit.
- A filter/model sample-rate mismatch, unstable coefficients, non-finite
  values, or unsupported model rejects the configuration.
- Room-correction boost is limited to approximately +3 dB in v1; cut is
  preferred. Exact enforcement is defined by the schema and validation task.
- Optional `auto_trim` is opt-in and capped at 12 dB. It must be reported;
  firmware must not silently change user gain.
- The conservative static response bound is `Σ |w| · |H|`; phase
  cancellation between input channels is not credited.

## Protection requirements

- Protected data is provisioned separately as `drv`, `amp`, and
  `supply`; it is not part of uploaded speaker JSON.
- Each physical output has a provisioned frequency-dependent limit curve.
- Runtime limiters are gain-only and cannot boost.
- Sealed-box excursion protection uses the approved model based on
  `fc_hz`, `Qtc`, and `mm_per_v`; unsupported or invalid models fail closed.
- A required subsonic high-pass stage must be present.
- User-authored limiter stages are rejected; the compiler owns mandatory
  protection stages.
- The final sample clamp must satisfy
  `abs(y) <= configured_ceiling`.
- The hardware volume/gain setting is pinned. Validate
  `vpk_at_0dbfs <= clip_v` against provisioned amplifier data.
- Missing or invalid protected data leaves outputs muted.

## Configuration lifecycle and safety

- Configuration changes are serialized. A concurrent mutation returns
  `E_BUSY` before building or writing a candidate.
- Persistence occurs only on an explicit user commit; slider edits and
  previews must not cause flash writes.
- The DSP owner acknowledges configuration transitions at a safe boundary.
  A timeout is fail-closed; the 250 ms value remains provisional pending
  human approval.
- `FAULT_MUTED` is currently assumed to remain latched until power-cycle and
  fresh validation. This is provisional and requires human safety review.
- Every start, stop, configuration transition, and fault uses controlled
  ramps and mute.
- No network/API path may modify protected data.

## Human-owned decisions

- Exact ESP32-S3 module, flash size, PSRAM availability, and memory budget.
- ESP-IDF version and target I2S TDM API.
- GPIO map, MCLK/BCLK/WS/DOUT, I2C pins, DAC board model, addresses, straps,
  and slot mapping.
- Driver protection limits and cited driver data.
- Amplifier voltage/clip data, hardware gain setting, and supply budget.
- Limiter detector constants, attack/release behavior, and any look-ahead.
- Flash endurance budget and final commit/persistence ordering.
- Hardware measurements and target timing/CPU budget.

No hardware value or protection limit may be inferred by an implementation
agent. See ADRs 0012 and 0013.
