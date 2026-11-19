# ADR 0008: Protection record encoding and pure evaluation
- Status: Accepted for planning; safety implementation requires human CODEOWNERS review
- Date: 2026-11-19

## Record format
Exactly one record per output node; unknown and duplicate IDs reject. out_id is char[32] including NUL. All values finite. max_gain_db is an absolute response ceiling. stop_below_hz=0.0f disables the lower-band check; stop_above_hz=0.0f disables the upper-band check. When disabled, its attenuation field MUST be 0.0f. Enabled edges are [10, 20000] Hz; attenuation is a positive amount [0,120] dB. No NaN, infinity, negative edge, sentinel, or missing record can disable protection. Missing/malformed/duplicate records fail E_PROTECT and output remains unavailable/muted. Real numeric ranges and actual driver limits remain human-owned.

For a band, compare magnitude dB to -atten_*_db, including edge: lower applies f<=edge, upper applies f>=edge. max_gain_db applies everywhere. Zero magnitude maps to -300 dB. Non-finite math fails closed.

## Pure evaluator
Core 0 only, deterministic, read-only, no NVS/network/hardware. Requires validated config and records. Every output must pass before publication. Visit outputs in config order, then ascending frequency; first failure reports E_PROTECT, output, frequency. Grid: existing 1054 points 10..20000 Hz plus exact 10, 20000 and each enabled edge, exact-equality dedupe; max 1070 points. Edges above 20 kHz invalid.

## Exact starter test vectors
Passthrough is 0 dB at every point. Gain -6.020599913 dB yields -6.020599913 dB within 1e-5 dB. Ceiling -1 rejects passthrough; ceiling 0 accepts. Lower edge 100 Hz / attenuation 20: passthrough rejects at 10 and exactly 100, accepts at 100.01 if ceiling allows. Upper edge 1000 / attenuation 20: rejects exactly 1000 and 20000, accepts 999.99. Disabled edge zero skips only its corresponding check. Missing, duplicate, unknown-output, NaN, infinity and nonzero disabled attenuation reject. These semantics vectors are not device limits.

Narrow high-Q features can fall between samples; this residual risk remains explicit. No task may alter this decision; conflicts stop and escalate.
