# ADR 0001: Host response evaluation contract
- Status: accepted
- Date: 2026-11-17
- Task: T04

## Context
T04 requires comparison of the response evaluator with SciPy over a logarithmic
10 Hz to 20 kHz grid for three filter families. A Python reimplementation can
pass without exercising the firmware C implementation.

## Decision
Expose `dsp_log_grid` as a core-0 API with exact endpoints and finite positive
ordered bounds. Build the same DSP sources as a host shared library and call
`dsp_design_*`, `dsp_log_grid`, and `dsp_sos_eval` from the SciPy acceptance
script. Keep magnitude and phase tolerances at 0.001 dB and 0.01 degrees.

## Alternatives considered
Golden response vectors, which do not cover the full grid at runtime; and a
Python copy of the evaluator, which does not test the compiled C implementation.

## Consequences (include measured data when changing a gate)
Host CI needs the pinned NumPy/SciPy requirements. The acceptance gate is
unchanged and now observes the compiled C implementation directly.
