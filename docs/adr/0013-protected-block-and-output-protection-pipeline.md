# ADR 0013: Protected block and output protection pipeline

- Status: Proposed; requires human safety review and CODEOWNERS approval.
- Date: 2026-11-21
- Depends on: ADR 0008, ADR 0010, and ADR 0012

## Protected block

The device has a separately provisioned protected block with three authorities:

- `drv`: driver capability and safety data for each physical output;
- `amp`: amplifier gain, voltage, clipping, and output capability data;
- `supply`: shared power-supply and total power-budget constraints.

The protected block is not part of the uploaded speaker-design JSON. It is
provisioned through the owner-controlled process, stored separately from
ordinary configuration, and cannot be changed by the phone UI, network API,
or a configuration upload. Every value must be sourced from an identified
datasheet, owner measurement, or explicit human decision. Missing or invalid
protected data keeps outputs muted.

The provisioned data is used to derive a per-output frequency-dependent
protection limit curve and the runtime limiter parameters. The exact storage
encoding and provisioning tool are separate implementation tasks.

## Static load-time validation

For every output, the compiler:
1. verifies that all mandatory protection keys and protected records exist;
2. validates all filter and model coefficients for finiteness and stability;
3. evaluates each input-channel-to-output transfer across the protection grid;
4. calculates the conservative stereo/multi-input bound `Σ |w| · |H|`;
5. compares that bound against the output's provisioned limit curve;
6. reports any required trim and applies it only when `auto_trim` is explicitly
   enabled and the required attenuation is no more than 12 dB;
7. rejects the candidate if any output still exceeds its limit.

The exact trim calculation and grid interpolation are frozen by the comparator
prototype. No response value may be inferred from a runtime limiter's
expected attenuation.

## Runtime protection stages

The compiler appends these mandatory gain-only stages to each output chain:

### Thermal / RMS limiter

Limits sustained energy according to the provisioned driver thermal data.
It can only reduce gain. Its detector, time constants, and numerical
implementation must be specified by the protection task bundle from
owner-approved values; agents must not invent thermal constants.

### Excursion limiter

For a sealed-box driver model, use the specified single low-pass biquad
derived from `fc_hz`, `Qtc`, and `mm_per_v`. The model is evaluated using
the protected driver's provisioned parameters. Coefficients must be finite
and stable at the configured sample rate. If the protected model is absent,
unsupported, or inconsistent, reject the candidate rather than bypassing
excursion protection.

### Peak limiter

Constrains instantaneous output peaks to the protected voltage ceiling.
It is gain-only and may not create positive gain. Attack/release behavior and
look-ahead requirements must be frozen in the limiter task bundle and
verified with deterministic vectors.

### Global power budget

A system-owned stage enforces the shared `supply` budget across outputs.
It must coordinate attenuation without allowing any individual output to
exceed its own protected ceiling. Its exact linked-gain algorithm is a
separate reviewed contract; no per-output limiter may bypass it.

### Final clamp and hardware volume

The final clamp is system-owned and guarantees:

[
|y[n]| \le configured\_ceiling
]

for every output sample. The hardware volume/gain setting is pinned to its
provisioned value. Validate `vpk_at_0dbfs <= clip_v` using protected
amplifier data before enabling output. If this relation fails or either value
is missing/non-finite, fail closed.

Ramp and mute remain system-owned and apply to start, stop, configuration
change, and fault transitions. They are not ordinary user chain stages.

## JSON protection configuration

Each output must include the mandatory protection configuration defined by
the Step 3 schema. The user may select permitted limiter operating parameters
within the provisioned limits, but may not disable a required limiter, increase
a protected ceiling, or alter `drv`, `amp`, or `supply` data.

The schema rejects missing, misspelled, unknown, wrong-unit, out-of-range, or
inconsistent protection keys. A limiter placed in the user-authored chain is
invalid because the compiler owns the mandatory protection pipeline.

## Failure behavior

Any missing or invalid protected record, invalid model, non-finite detector
state, invalid limiter configuration, static curve violation, clamp
configuration violation, or hardware output fault keeps or returns the
system to muted fault state. No API operation can clear a latched fault.
The current assumption is that `FAULT_MUTED` remains latched until a
power-cycle/reinitialization and fresh validation; this requires human
approval.

## Verification status

Protection architecture proposal only. No driver values, amplifier values,
supply limits, limiter algorithms, provisioning format, or hardware behavior
are claimed verified.
