# ADR 0008: Per-output protection curves and static safety evaluation

- Status: Proposed planning contract; supersedes the former 52-byte stop-band record.
- Date: 2026-11-21
- Depends on: ADR 0012 and ADR 0013
- Human safety review and CODEOWNERS approval are required before implementation.

## Decision

Protection is evaluated per physical output using a provisioned frequency-
dependent limit curve. The former record fields
`max_gain_db`, `stop_below_hz`, `atten_below_db`,
`stop_above_hz`, and `atten_above_db` are retired. A zero-valued band edge
must never be used as a way to disable protection.

The protection curve is derived from the separately provisioned protected
block (`drv`, `amp`, and `supply`) described in ADR 0013. Uploaded JSON
cannot create, replace, weaken, or disable that protected block.

Each configured output must have exactly one provisioned protection curve.
Missing, duplicate, malformed, non-finite, or unknown output records reject
the complete configuration and leave outputs muted. Every output must pass
before any candidate can be activated.

## Static response bound

For each output and each evaluation frequency, the compiler evaluates the
complex transfer from each configured input channel to that output, including
the optional input pre-chain and the output's ordered user chain.

For input channel (i), let (w_i) be its configured mix weight and
(H_i(f)) its complex transfer to the output. The safety comparator uses the
conservative bound:

```text
A_bound(f) = sum_i(abs(w_i) * abs(H_i(f)))
```

The comparator does not credit phase cancellation between inputs. This is
deliberately conservative and is the required stereo-sum rule. Compare the
bound, converted to dB, against that output's provisioned limit curve at the
same frequency. A response that exceeds the curve rejects the candidate.

The calculation includes all static user filters and gains. Runtime limiter
action is not used to excuse a statically unsafe response. Any unsupported,
non-finite, unstable, or un-evaluable stage fails closed.

## Evaluation grid and curve validity

- Frequencies are finite, positive, and strictly increasing.
- The evaluation covers the configured audio band, initially 10 Hz to
  20 kHz at the existing 1054-point logarithmic grid, plus exact provisioned
  curve knots and any required exact model boundaries.
- Duplicate frequencies are deduplicated exactly; the final evaluation list
  is strictly increasing.
- Curve values are finite and must cover the full evaluation range. No
  extrapolation outside the provisioned curve is permitted.
- The final maximum point count and interpolation rule must be frozen with
  the comparator task before implementation. They are not to be guessed by
  an implementation agent.

## Optional automatic trim

The evaluator reports the worst required attenuation when a candidate
exceeds its curve. If `auto_trim` is explicitly enabled, the builder may add
a single non-positive output trim and repeat the complete static evaluation.
The trim is capped at 12 dB. If more than 12 dB is required, or the second
evaluation still fails, reject the candidate. With `auto_trim` absent or
false, do not silently change user gain.

The exact JSON spelling and report structure are frozen in the schema and
comparator task, not in this ADR.

## Runtime protection

The compiler appends mandatory protection stages after the user-authored
output chain. Users cannot insert, remove, reorder, or bypass these stages.
Runtime limiters are gain-only: they may attenuate but must never boost.
The v1 protection stages are:
1. thermal/RMS limiter;
2. sealed-box excursion limiter where the protected driver model requires it;
3. peak limiter;
4. final hard clamp, followed by the system-controlled ramp/mute path.

The sealed-box excursion model is a single low-pass biquad derived from the
protected driver's `fc`, `Qtc`, and `mm_per_v` data. Unsupported driver
models fail closed; they are not approximated by the implementer.

The final clamp invariant is `abs(y) <= configured_ceiling` for every sample.
The ceiling is bounded by the protected amplifier/supply data. No runtime
limiter may raise the ceiling or bypass the clamp.

## Required rejection cases

The new schema and tests must reject at least:
- missing or misspelled mandatory protection configuration keys;
- unknown protection keys;
- unstable or non-finite excursion-model coefficients;
- a filter sample-rate mismatch;
- a conservative stereo-sum bound above the provisioned curve;
- a missing required subsonic high-pass stage;
- a user-authored limiter inside an ordinary output chain;
- `vpk_at_0dbfs > clip_v`;
- missing, duplicate, unknown, incomplete, or non-finite provisioned output
  curves.

## Verification status

Planning contract only. No new comparator, limiter, curve generator, hardware
limit, or runtime behavior is claimed implemented or verified.
