# Architecture

## Product shape

A JSON-configured active crossover runs on the ESP32-S3 at 48 kHz. The first
configuration is a 2.1 speaker system; the architecture must scale to 5.1 and
other multichannel layouts.

VituixCAD or another external speaker-design tool calculates crossover and
equalization values. The ESP32 validates and executes those supplied values.
It does not derive driver limits, redesign the speaker, or guess amplifier
capabilities.

## Signal path

```text
Input channels
  -> optional per-input pre-chain (shared room correction / tilt)
  -> per-output input mix (explicit channels and weights)
  -> ordered per-output DSP chain
  -> compiler-appended mandatory protection stages
  -> system-wide power budget
  -> final clamp
  -> system ramp / mute
  -> I2S TDM output and DACs
```

There is no user-authored DSP graph or DAG. Each output declares its input
mix and an ordered list of processing stages. An optional pre-chain belongs
to an input channel and may be shared by multiple output mixes. Each output
has independent state for its own stateful stages.

## Configuration and compilation

The uploaded JSON is a declarative speaker design. Its top-level version and
sample-rate keys are mandatory. Every output declares:
- the input channels and linear weights to mix;
- its ordered user DSP chain;
- the mandatory protection configuration required by the schema.

Unknown keys, missing mandatory protection keys, wrong units, invalid values,
and unsupported stages are rejected. The exact v1 shape is defined by
`configs/schema.json` and its fixtures; the schema migration is a separate
planned step.

The core-0 builder validates the complete configuration and compiles each
output into a bounded execution table. Each entry identifies a precompiled
stage function, immutable parameters, and its runtime state. The DSP task
walks each output table in declared order. JSON parsing, coefficient design,
allocation, logging, locks, and unbounded work are forbidden in the real-time
path.

Existing `dspblocks` filter design, SOS processing, gain, delay, polarity,
mixing, numeric-safety, and response primitives are reused where their
contracts and tests remain valid. The graph-oriented `dspgraph` runtime is
retired for this product architecture. A focused `dspchain` component
replaces its build/execute role; its exact C interface and memory contract
must be approved before implementation.

## Protection boundary

Driver, amplifier, and supply facts live in a separately provisioned
protected block (`drv`, `amp`, `supply`). The uploaded configuration and
network API cannot change those values.

At load time, the compiler evaluates each input-channel-to-output response
and applies the conservative bound `Σ |w| · |H|`. It compares the result
against the provisioned per-output frequency-dependent limit curve. Phase
cancellation is not credited. An unsafe or incomplete configuration is
rejected before activation.

The compiler appends mandatory gain-only thermal/RMS, excursion (when
required by the protected driver model), and peak limiters. A system-owned
global power budget and final clamp follow. The final clamp guarantees
`abs(y) <= configured_ceiling` for every output sample. Ramp and mute are
system-owned and cannot be removed or reordered by uploaded JSON.

## Runtime ownership

- Core 0: Wi-Fi, HTTP, parsing, validation, filter design, chain compilation,
  storage, and response reporting.
- Core 1: bounded DSP processing and TDM output service.
- The DSP task exclusively owns live filter/limiter state.
- Input and output buffers and compiled state are allocated before real-time
  processing; no heap allocation occurs in the audio path.
- Configuration changes use an owner-acknowledged safe transition. A prior
  generation is not reclaimed until the DSP owner can no longer reference it.
- Configuration persistence occurs only on an explicit user commit. The
  exact preview/apply/commit ordering remains subject to human review.

## Verification status

This is an architecture description, not evidence of a working target audio
path. Hardware pinout, exact DAC board/straps, clocking, amplifier data,
driver limits, memory capacity, and target timing remain human-verified
requirements. See ADRs 0012 and 0013 for the normative architecture and
protection proposals.
