# API contracts

Status: existing dspblocks APIs remain frozen. The graph/DAG JSON format and
dspgraph build/process API are legacy-only. The replacement dspchain
execution-table API is not yet frozen. Public header changes require an ADR,
this document update, tests, and CODEOWNERS review.

## 1. Error categories

| code | meaning |
|---|---|
| E_JSON | not parseable, too deep, or too large |
| E_SCHEMA | missing, unknown, wrong-type, wrong-unit, or out-of-range key |
| E_NODE_PARAM | invalid or unstable filter parameter |
| E_CHAIN | invalid output chain or stage ordering |
| E_INPUT_MIX | invalid input source, weight, or mix definition |
| E_LIMIT | requested value outside declared operating limits |
| E_PROTECT | invalid or incomplete protection configuration |
| E_RESOURCE | too many sections, stages, or required bytes |
| E_UNSUPPORTED | valid but unsupported filter or operation |
| E_AUTH | missing or wrong write token |
| E_BUSY | configuration transaction or safe transition is in progress |

Final error names and HTTP mapping must be frozen with the chain loader.
Do not weaken existing validation expectations to make an implementation
pass.

## 2. C modules

dspblocks/include/dspblocks.h is authoritative for filter design, SOS
processing, simple blocks, numeric safety, and response evaluation.
dsp_sos_eval and dsp_log_grid are core-0-only. Existing golden vectors must
not be regenerated to accommodate an implementation.

The future dspchain builder validates JSON and compiles optional per-input
filter stacks and ordered per-output execution tables. Each entry references
a precompiled operation, immutable parameters, and allocated state. Public
C types, capacities, memory ownership, builder/loader API, and statistics
snapshot are not frozen. Do not reuse legacy dspg_* signatures or invent an
API while implementing an unrelated task. JSON cannot remove or reorder
mandatory protection and system-owned stages.

The audiosrc, tdmout, and pcm5122 component boundaries are retained. No
decoder or network work is allowed in the DSP task. Hardware pin, clock,
I2S, DAC register, I2C address, strap, and slot facts require verification.

## 3. Configuration file

JSON v1 is defined by configs/schema.json. Top-level fields are v=1, fs=48000,
declared inputs, optional pre, required outputs, and optional protection.
Each pre-chain has channel and filters[]. Each output has id, slot, ordered
in[] mix, filters[], gain_db, polarity, and delay_samples. Every filter has
type, f_hz, q, and gain_db. The schema owns allowed types and numeric bounds.
Firmware expands filters to biquad sections once during configuration load.

Optional protection contains one record per output: id, weighting.filters[],
peak_threshold_dbfs, rms_threshold_dbfs, rms_tau_ms, release_ms, and
ceiling_dbfs. Protection IDs must match output IDs exactly. The schema is
the single source of truth for numeric bounds. Missing or incomplete
protection means every output remains muted. Staging configurations without
protection are structurally valid but are not enabled configurations.

Unknown fields reject. Driver T/S, amplifier, and supply models do not belong
in device JSON.

## 4. HTTP and configuration lifecycle

The control interface remains LAN-only with bounded JSON requests and
bearer-token authentication for writes. Exact endpoint paths and payload
shapes must be updated with the explicit-commit UI design; the old graph
variable endpoint is not normative.

All configuration mutations share one transaction gate. Concurrent mutation
returns E_BUSY before candidate build or storage writes. Preview or slider
movement must not persist configuration. Flash persistence occurs only on
explicit user commit. A committed candidate must pass schema, semantic,
filter-design, protection, and resource validation before activation. The DSP
owner acknowledges activation at a safe boundary. A rejected or uncertain
transition fails closed.

## 5. Persistent storage

- User configuration uses the A/B slot concept with header, version, length,
  and CRC validation.
- An inactive slot is not active merely because it was staged. Selector
  publication follows the human-approved explicit-commit protocol.
- A/B slot CRC is the integrity mechanism for stored configuration bytes.
  Do not add a second protection-object CRC or require JSON canonicalization.
- Flash endurance and final commit ordering remain human decisions.

## Planning authority

- ADR 0012: chain-per-output architecture and compiled execution tables.
- ADR 0014: current firmware protection contract and safety boundary.
- ADRs 0008, 0010, and 0013: superseded historical decisions only.
- configs/schema.json: normative JSON shape and numeric bounds.

On conflict between this document, an ADR, source headers, or a task bundle,
stop and escalate. No task may infer a missing signature or safety value.
