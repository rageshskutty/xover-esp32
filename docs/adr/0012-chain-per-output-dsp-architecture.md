# ADR 0012: Chain-per-output DSP architecture

- Status: Proposed; architecture and migration plan require maintainer review.
- Date: 2026-11-21
- Supersedes: graph/DAG execution model in ADR 0007 and graph-oriented portions
  of docs/architecture.md, docs/design.md, and docs/api.md.

## Context

The product is a JSON-configured active crossover running on ESP32-S3 at
48 kHz. VituixCAD or another external speaker-design tool calculates the
crossover and equalization values. The ESP32 validates and executes the
supplied configuration; it does not invent driver limits or redesign the
speaker.

The initial deployment is 2.1. The architecture must extend to 5.1 and other
multichannel layouts without requiring a general-purpose graph language.

## Decision: ordered chains, not a general graph

The configuration consists of:
- optional shared pre-processing chain(s) on input channels;
- one independently configured input mix for each output;
- one ordered user DSP chain for each output;
- a compiler-appended, mandatory protection pipeline;
- system-owned power-budget, clamp, ramp, and mute stages.

Conceptual signal path:

```text
Input L/R
  └─ optional per-input pre-chain (shared room correction / tilt)
       └─ per-output input mix (explicit source channels and weights)
            └─ ordered per-output user DSP chain
                 └─ mandatory protection pipeline
                      └─ global power budget
                           └─ final clamp
                                └─ system ramp / mute
                                     └─ DAC output
```

Each output is a self-contained ordered sequence. The configuration does not
contain arbitrary nodes, edges, fan-out, fan-in rules, or a DAG. The compiler
does not topologically sort a graph. An output's `in` field declares its
input mix; its `chain` field declares the ordered stages.

An optional input pre-chain is processed once for its input channel and may
feed multiple output mixes. Each output's stateful stages have independent
state. No stateful filter state is shared between output chains. The exact
ownership and buffer-lifetime contract is specified with the `dspchain`
component before implementation.

## Compiled execution table

The builder validates the complete JSON configuration on core 0 and compiles
it into bounded per-output execution tables. Each table entry identifies the
precompiled DSP stage function, immutable stage parameters, and its allocated
runtime state. The DSP task walks each output table in declared index order.
No JSON parsing, filter design, allocation, logging, locks, or unbounded
work is permitted in the real-time processing path.

Existing `dspblocks` filter design, SOS processing, gain, delay, polarity,
mix, numeric-safety, and response primitives are reused where their existing
contracts and tests remain valid. The old `dspgraph` component is not the
basis of the new runtime and must not receive new graph features. A new
`dspchain` component replaces its graph-building role. The exact C interfaces,
capacity limits, memory ownership, and stage descriptor layout are deferred
to the dedicated component contract and task cards.

## Input mix and response semantics

Each output's input mix explicitly lists the contributing input channels and
their linear weights. The order is stable and preserved from JSON. No implicit
stereo summing, polarity inversion, channel duplication, or delay alignment is
introduced. The speaker designer supplies the intended mix and filter values.

For static protection, evaluate each input-channel-to-output transfer
separately, including that input's pre-chain and the output chain. Apply the
conservative magnitude bound `Σ |w| · |H|` from ADR 0008. Do not use phase
cancellation to claim a safer output.

## Mandatory protection and system stages

Users may configure ordinary DSP stages only. The compiler appends the
mandatory thermal/RMS, excursion (when required by the protected driver
model), and peak limiter stages. A user cannot insert a limiter in the ordinary
chain, remove a mandatory stage, change its order, or bypass it.

The system-owned global power budget, final clamp, ramp, and mute stages are
not user chain entries. Their exact runtime coordination and buffer ordering
must be frozen before implementation. The final sample clamp must satisfy the
configured output ceiling for every sample.

## Configuration contract direction

The next schema revision uses the compact top-level keys `v` for the
configuration version and `fs` for sample rate. Protection configuration
keys are mandatory, unit-bearing, and strictly validated. Unknown keys are
rejected. Exact JSON shapes, required keys, parameter bounds, and invalid
fixtures are defined in Step 3; this ADR does not authorize an implementation
to guess them.

Frequency, voltage, time, and displacement units must be explicit in key
names. The sample rate must match the device's fixed supported rate. A
biquad or model whose declared sample rate differs from the active sample
rate is rejected.

## Frozen JSON v1 shape (Step 3)

The top-level object is closed and contains:
- `v: 1` and `fs: 48000` (both constants in v1);
- `inputs`, a non-empty unique array drawn from `L, R, C, LFE, SL, SR`;
- optional `pre`, an array of `{ "channel": "<input>", "chain": [...] }`;
- `outputs`, 1–8 output objects.

Each output has `id`, `slot`, `in`, and `chain`. `id` is the join key to the protected record's `out_id`. The sets must match one-to-one: no missing output record and no orphan protected record. `slot` has only a schema sanity range of 0–31; the provisioned TDM slot count is authoritative and is checked by semantic validation.

The `pre` and `in` arrays use explicit channel names. `pre[].channel` is unique. Each `in[].channel` must be declared in top-level `inputs` and is unique within its output. An input channel with no `pre` entry is an identity path. An `in` entry therefore selects the post-pre-chain signal for that channel. This exact signal is used when calculating the per-input transfer and the conservative protection sum.

Each output has 1–8 `in` entries. Every weight is finite, non-zero, and in [-4, 4]; negative weights are valid. Protection uses the absolute weight, never its sign. Output IDs and slots are unique. These cross-field and uniqueness rules are semantic validation rules and report `E_SCHEMA`.

The ordered `chain` contains closed block objects:
- biquad: `{ "t": "biquad", "k": "lp|hp|peq|notch|allpass", "f_hz": number, "q": number }`; PEQ additionally requires `db` and may include optional `role`. `role` is not accepted on other block kinds;
- gain: `{ "t": "gain", "db": number }`;
- delay: `{ "t": "delay", "us": number }`;
- polarity: `{ "t": "polarity", "invert": boolean }`;
- SOS: `{ "t": "sos", "fs": 48000, "b": [b0,b1,b2], "a": [a1,a2] }`.

The exact field bounds and closed object definitions are in `configs/schema.json`. The schema permits an empty `chain`; semantic protection validation rejects it with `E_PROTECT` because no attenuation below the protected `min_freq_hz` can be established. More generally, static protection validation—not JSON Schema—must establish adequate attenuation below each output's protected minimum frequency.

## Non-goals

- A user-authored DAG or arbitrary graph editor.
- Runtime filter design or coefficient derivation from driver data.
- FIR, feedback graphs, or implicit phase/polarity compensation.
- Allowing uploaded JSON or network APIs to alter the protected block.
- Implementing the new schema or DSP engine as part of this ADR.

## Migration

Retain existing tested `dspblocks` implementation and golden vectors.
Replace the graph-oriented configuration schema and `dspgraph` execution
contract incrementally with `dspchain`. Graph-specific fixtures such as
cycle detection and fan-in-without-mix are retired because those structures
no longer exist. Preserve unrelated validation, numeric-stability, resource,
and fail-closed tests by translating them to chain-specific equivalents.

No source component is deleted in this ADR-only planning change. The
implementation plan must identify the exact files to reuse, replace, or retire
before code changes begin.

## Verification status

Architecture proposal only. No schema, compiler, chain executor, limiter, or
hardware path has been implemented or verified.
