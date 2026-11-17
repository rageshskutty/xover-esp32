# ADR 0002: Simple DSP blocks and numeric safety contract
- Status: accepted
- Date: 2026-11-17
- Task: T05

## Context
T05 requires real-time building blocks (gain with ramp, polarity, integer-sample
delay, mix) and numeric safety guarantees (NaN/Inf detection per block output,
denormal handling verified with a 10-second decaying tail).

## Decision
Implement RT-safe execution in `blocks_rt.c`:
- Gain: monotonic per-sample ramp tracking target and remaining frames.
- Polarity: in-place or out-of-place sign inversion.
- Delay: circular buffer delay exact to the sample up to capacity.
- Mix: multi-input weighted summation with per-input linear gains.
- Numeric safety: bitwise NaN/Inf detection (`dsp_is_nan_or_inf`), denormal
  flushing (`dsp_flush_denormal`), and block sanitizer (`dsp_sanitize_block`)
  muting buffers to zero on NaN/Inf fault.
- Stateful filter protection in `dsp_sos_rt.c`: reset states on non-finite input
  and flush denormal states.

## Consequences (include measured data when changing a gate)
All block and safety functions are RT-safe (no allocation, no double precision,
no locks). Host test `test_blocks` verifies delay exactness, monotonic ramping,
NaN muting, and absence of subnormals across 480,000 samples (10 s).
