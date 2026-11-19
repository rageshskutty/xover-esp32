# ADR 0011: Graph response, variable updates, and stats API
- Status: Accepted for planning; implementation requires CODEOWNERS review
- Date: 2026-11-19
- Supersedes: response, variable-update, and graph-stats sketches in docs/api.md
  and the state-ownership sentence in ADR 0007

## Decision
The public API in `firmware/components/dspgraph/include/dspgraph.h` is the
single C declaration surface for `dspg_protect_t`, `dspg_set_var`,
`dspg_eval`, and `dspg_stats_t`. Protection records are passed by value
through this public contract; the same type is not redeclared in an internal
header. The 7B may implement the declarations only after this ADR and
`docs/api.md` agree. No production header or source is changed by this ADR.

### State ownership
Each stateful graph node owns one independent state instance per audio channel
(0 and 1) that reaches it. A node-channel pair is processed once per block.
Fan-out reuses that node-channel's computed block; it never clones or advances
the state once per downstream output path. Different node IDs remain distinct
even when their parameters match. Mix sums its ordered input blocks for the
same channel, with float32 left-to-right accumulation. This replaces the
ambiguous “per channel and output path” rule in ADR 0007. Stateless nodes need
no state. Reset clears every allocated node-channel state.

### Response evaluation
`dspg_eval` is core-0-only and read-only. It evaluates the active graph's
linear complex response, in stable topological order, at caller-supplied
finite, strictly increasing frequencies in [10, 20000] Hz. Each filter uses
its designed coefficients; gain, polarity and delay apply their corresponding
complex transfer. Mix complex-sums input responses in declared edge order.
Fan-out reuses the upstream complex result. Return magnitude in dB and phase
in degrees in [-180,180); zero magnitude is -300 dB and 0 degrees. Invalid
IDs, dimensions, frequencies, or non-finite intermediate/output values fail
closed and do not return partial arrays. This function is for host/HTTP
response reporting; protection evaluation remains governed by ADRs 0008/0010.

### Variable update
`dspg_set_var` never mutates the active graph. It creates a complete candidate
graph from the active graph's retained validated config and protection-record
snapshot, applying one declared variable override within its declared limits.
It reuses the same parser, validator, builder, and protection evaluator as
`dspg_build`. The candidate is unpublished and has fresh node-channel state.
On every failure, candidate output is NULL and the active graph is unchanged.
The caller persists the candidate config through the A/B slot protocol and
publishes it only at the existing owner-acknowledged swap boundary. No
network/API path can alter the protection snapshot.

### Stats seam
`dspg_stats_t` is a snapshot of graph-runtime counters only:
`blocks_processed`, `nonfinite_faults`, `clips`, `underruns`,
`cycles_total`, and `cycles_max`. Counters start at zero for a newly built
graph and are updated by the DSP owner. The snapshot API is DSP-owner-only;
cross-task status reporting copies the snapshot through the existing bounded
message/ring boundary. System-wide CPU percentiles and HTTP state remain
application-owned and are not fields of `dspg_stats_t`.

## Exact C seams
```c
typedef struct {
    char out_id[32];
    float max_gain_db;
    float stop_below_hz, atten_below_db;
    float stop_above_hz, atten_above_db;
} dspg_protect_t;

typedef struct {
    uint32_t blocks_processed;
    uint32_t nonfinite_faults;
    uint32_t clips;
    uint32_t underruns;
    uint64_t cycles_total;
    uint32_t cycles_max;
} dspg_stats_t;

int dspg_set_var(const dspg_t *active, const char *name, double value,
                 dspg_arena_t candidate_arena, dspg_t **candidate,
                 dspg_info_t *info, dspg_report_t *report);
int dspg_eval(const dspg_t *g, const char *out_id,
              const double *frequency_hz, size_t n_frequencies,
              double *magnitude_db, double *phase_deg,
              dspg_report_t *report);
int dspg_get_stats(const dspg_t *g, dspg_stats_t *stats);
```
These signatures are normative. `dspg_protect_t` is declared in the public
header and is not duplicated in an internal header. `dspg_build` remains as
specified in ADR 0007. `dspg_set_var` and `dspg_eval` return 0 on success and
-1 on failure. `dspg_get_stats` is called only by the DSP owner and returns
0 on success, -1 for null input.

## Verification
Planning contract only. No implementation or response vectors verified.
