# ADR 0008: Protection record encoding and pure evaluation
- Status: Accepted for planning; safety implementation requires human CODEOWNERS review
- Date: 2026-11-19

## Record format
Exactly one record per output node; unknown and duplicate IDs reject. `out_id` is
`char[32]` including NUL. All values finite. `max_gain_db` is an absolute
response ceiling. `stop_below_hz=0.0f` disables the lower-band check;
`stop_above_hz=0.0f` disables the upper-band check. When disabled, its
attenuation field MUST be `0.0f`. Enabled edges are [10, 20000] Hz;
attenuation is a positive amount [0,120] dB. No NaN, infinity, negative edge,
sentinel, or missing record can disable protection. Missing/malformed/duplicate
records fail E_PROTECT and output remains unavailable/muted. Real numeric
ranges and actual driver limits remain human-owned.

For a band, compare magnitude dB to `-atten_*_db`, including edge: lower
applies `f<=edge`, upper `f>=edge`. `max_gain_db` applies everywhere. Zero
magnitude maps to -300 dB. Non-finite math fails closed.

## Pure evaluator
Core 0 only, deterministic, read-only, no NVS/network/hardware. Requires
validated config and records. Before comparing any response sample, validate
the complete input and record set, including a one-to-one correspondence
between output IDs and records. Every output must pass before publication.
Visit outputs in config order, then ascending frequency; first response-limit
failure reports E_PROTECT, output, frequency. Grid: existing 1054 points
10..20000 Hz plus exact 10, 20000 and each enabled edge, exact-equality
dedupe; max 1070 points. Edges above 20 kHz invalid.

Input with `n_frequencies==0` or `n_outputs==0` is malformed and returns -1.
Any non-finite response sample returns -1. On every malformed input (including
a non-finite sample), zero the entire failure record; therefore
`failure->out_id` is empty. A malformed record is not a response-limit
failure and is handled before comparisons. Valid finite response-limit
failures report the first failing output and frequency in deterministic order.

## Exact starter test vectors
Passthrough is 0 dB at every point. Gain -6.020599913 dB yields -6.020599913
dB within 1e-5 dB. Ceiling -1 rejects passthrough; ceiling 0 accepts. Lower
edge 100 Hz / attenuation 20: passthrough rejects at 10 and exactly 100,
accepts 100.01 if ceiling allows. Upper edge 1000 / attenuation 20: rejects
exactly 1000 and 20000, accepts 999.99. Disabled edge zero skips only its
corresponding check. Missing, duplicate, unknown-output, NaN, infinity and
nonzero disabled attenuation reject. These semantics vectors are not device
limits.

Bijection vector: output IDs `{o,o}` and record IDs `{o,p}` reject even
though counts match; validating only each output's first matching record is
insufficient. Also reject zero outputs, zero frequencies, non-finite response
samples, non-monotonic frequencies, and malformed IDs. For malformed input,
the full failure record is zero, including an empty output ID.

Narrow high-Q features can fall between samples; this residual risk remains
explicit. No task may alter this decision; conflicts stop and escalate.
