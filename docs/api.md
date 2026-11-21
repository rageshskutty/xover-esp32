# API contracts

Status: existing `dspblocks` APIs remain frozen. The former `dspgraph`
build/process API and graph JSON format are retired for the product
architecture. The replacement `dspchain` API, configuration schema, and
HTTP commit contract are pending the planned implementation-contract work.
Public header changes require an ADR, this document update, and CODEOWNERS
review.

## 1. Error codes

The following semantic categories remain relevant to the new chain loader:

| code | meaning |
|---|---|
| E_JSON | not parseable, too deep, or too large |
| E_SCHEMA | missing, unknown, wrong-type, wrong-unit, or out-of-range key |
| E_NODE_PARAM | invalid or unstable filter/stage parameter |
| E_CHAIN | invalid output chain or stage ordering |
| E_INPUT_MIX | invalid input source, weight, or mix definition |
| E_VAR_UNDEFINED | undefined variable, if retained by the final schema |
| E_LIMIT | requested value outside declared operating limits |
| E_PROTECT | static response or protected-block violation |
| E_RESOURCE | too many stages, sections, or required bytes |
| E_UNSUPPORTED | valid but unsupported stage or protected driver model |
| E_AUTH | missing or wrong write token |
| E_BUSY | configuration transaction or safe transition is in progress |

The former graph-only codes `E_GRAPH_REF`, `E_GRAPH_CYCLE`,
`E_GRAPH_FANIN`, and `E_GRAPH_DEAD` are retired with the DAG format.
Final error names and HTTP mapping must be frozen with the chain loader and
schema tasks. Existing host validator error expectations must be migrated
without weakening unrelated validation.

## 2. C modules

### 2.1 dspblocks (existing frozen DSP primitives)

The existing public header
`firmware/components/dspblocks/include/dspblocks.h` remains the authority
for filter design, SOS processing, simple blocks, numeric safety, and
frequency-response evaluation.

The following signatures summarize the existing surface; do not change them
without a separate ADR and CODEOWNERS review:

```c
typedef enum { DSP_OK = 0, DSP_E_PARAM, DSP_E_RANGE, DSP_E_UNSTABLE,
               DSP_E_UNSUPPORTED, DSP_E_NOMEM } dsp_err_t;
typedef struct { float b0, b1, b2, a1, a2, da1, da2; } dsp_sos_t;
typedef struct { float z1, z2; } dsp_state_t;
#define DSP_MAX_SOS 4
typedef struct { dsp_sos_t sos[DSP_MAX_SOS]; uint8_t n; } dsp_cascade_t;

dsp_err_t dsp_design_xo(dsp_family_t, dsp_mode_t, unsigned order,
                        double fc_hz, double fs_hz, dsp_cascade_t *out);
dsp_err_t dsp_design_peq(double f_hz, double q, double gain_db,
                         double fs_hz, dsp_sos_t *out);
dsp_err_t dsp_design_shelf(dsp_shelf_t, double f_hz, double gain_db,
                           double fs_hz, dsp_sos_t *out);
dsp_err_t dsp_design_allpass(double f_hz, double q,
                             double fs_hz, dsp_sos_t *out);
dsp_err_t dsp_design_notch(double f_hz, double q,
                           double fs_hz, dsp_sos_t *out);
dsp_err_t dsp_check_stable(const dsp_sos_t *s);

void dsp_sos_process(const dsp_sos_t *s, dsp_state_t *st,
                     size_t n_sections, const float *in, float *out,
                     size_t n_frames);
```

Simple block, numeric-safety, and response-evaluation declarations remain
those in the existing public header. Core-0-only design/evaluation and
real-time restrictions in AGENTS.md remain in force. Existing golden vectors
must not be regenerated to make a new implementation pass.

### 2.2 dspchain (replacement component; contract pending)

The `dspchain` component replaces the graph-oriented build/execute role.
Its builder will validate and compile optional per-input pre-chains and
ordered per-output tables. Each table entry contains a precompiled stage
function, immutable parameters, and allocated state. The DSP task executes
each output table in declared order.

The exact public C types, stage descriptor, builder/loader API, memory
ownership, maximum capacities, response API, and statistics snapshot are
not frozen here. They are specified in the dedicated `dspchain` contract
and task cards. Do not reuse `dspg_*` signatures or invent a new API while
implementing an unrelated card.

The builder must append mandatory protection stages and system-owned output
stages. User JSON cannot remove, reorder, or bypass them. See ADRs 0012 and
0013.

### 2.3 audiosrc

The existing component boundary is retained. Its exact implementation and
threading contract must be reconciled with the final DSP runtime contract.
No decoder or network work is allowed in the DSP task.

### 2.4 tdmout and pcm5122

The existing component boundaries are retained. Pin assignments, I2S API,
DAC register values, I2C addresses, straps, and slot offsets remain
human/datasheet verified inputs. Do not guess hardware values.

## 3. Configuration file

The graph JSON format in the current schema is retired. The replacement
format uses top-level `v` for configuration version and `fs` for sample
rate. It defines optional input pre-chains and per-output input mixes and
ordered chains. Each output includes mandatory protection configuration.

JSON v1 is defined in `configs/schema.json` with a 2.1 example in
`configs/example_2way_sub.json`. It uses `v`, `fs`, `inputs`, optional `pre`,
and `outputs` containing `id`, `slot`, `in`, `chain`, and optional `protect`.
The schema handles closed local shapes; cross-field uniqueness, channel
membership, protected-record joins, protected limiter bounds, and static
response sufficiency are semantic validation. See ADRs 0012 and 0013.

Unknown keys reject. Physical units are included in key names. Uploaded
configuration cannot modify the separately provisioned `drv`, `amp`, or
`supply` protected block.

## 4. HTTP and configuration lifecycle

The device control interface remains LAN-only with bounded JSON requests and
bearer-token authentication for writes. Exact endpoint paths and payload
shapes must be updated with the explicit-commit UI design; the old
`PUT /vars/{name}` graph endpoint is not normative.

All configuration mutations share one transaction gate. Concurrent mutation
returns `E_BUSY` before building a candidate or writing storage. Preview or
slider movement must not persist configuration. Flash persistence occurs
only on an explicit user commit.

A committed candidate must pass schema, semantic, static protection, and
resource validation before activation. The DSP owner acknowledges activation
at a safe boundary. A rejected or uncertain transition fails closed. The
proposed 250 ms acknowledgement timeout is provisional and requires human
approval. See ADR 0011 for the transaction ordering and unresolved preview /
apply / commit details.

No HTTP or network path can modify protected data or provisioned limit curves.

## 5. Persistent storage

- The `drv`, `amp`, and `supply` protected block is stored separately
  from ordinary user configuration and is writable only through the
  owner-controlled provisioning process.
- User configuration uses the existing A/B slot concept, with header,
  version, length, and CRC validation.
- An inactive slot is not considered active merely because it was staged.
  Selector publication follows the human-approved explicit-commit protocol.
- The flash endurance budget and final commit ordering are open human
  decisions. A one-second write-rate floor is not accepted as proof of flash
  endurance.

Exact partition sizes and flash facts remain unverified until confirmed for
the selected module.

## Frozen planning authority

- ADR 0012: chain-per-output architecture and compiled execution tables.
- ADR 0013: protected block and mandatory protection pipeline.
- ADR 0008: per-output limit curves and static conservative response bound.
- ADR 0010: pure protection comparator contract and Step 4 prototype.
- ADR 0011: configuration mutation, explicit commit, and response ownership.
- ADR 0009 timing material, if present in the approved planning baseline,
  remains separate from the chain architecture.

On conflict between this document, an ADR, source headers, or a task bundle,
stop and escalate. No implementation task may infer a missing signature or
safety value.
