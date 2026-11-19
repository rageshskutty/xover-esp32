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
linear complex response in stable topological order at caller-supplied finite,
strictly increasing frequencies in [10, 20000] Hz. The caller selects
`input_channel` (0 or 1) as well as `out_id`. For each frequency, the selected
input channel is driven with a unit complex signal and the other input channel
with zero; all input nodes assigned to the selected channel receive that same
unit signal. The result is the transfer from that channel to the named output,
including every path and mix contribution in declared edge order. Thus a
stereo graph is evaluated as separate input-to-output transfers; it is never
implicitly collapsed into one response. Each filter uses its designed
coefficients; gain, polarity and delay apply their corresponding complex
transfer. Fan-out reuses the upstream complex result. Magnitude is
`20*log10(max(|H|, 1e-15))` in dB, so any |H| below 1e-15 (including exact
zero) reports -300 dB and phase 0 degrees. Otherwise phase is
`atan2(imag, real)` in degrees, and a result >= 180.0 is mapped to -180.0 so
the range is exactly [-180,180). `atan2` returns +180 for a real negative
value with +0.0 imaginary part and -180 with -0.0, so the mapping is
required: a lone inverted path reports -180 degrees for either sign of zero.
Invalid IDs, input channels, dimensions, frequencies, or non-finite
intermediate/output values fail closed and do not return partial arrays. This
function is for host/HTTP response reporting; protection evaluation remains
governed by ADRs 0008/0010.

Required response vectors: an identity route from input channel 0 to an output
returns 0 dB/0 degrees for input_channel=0 and -300 dB/0 degrees when only
input channel 1 reaches that output; evaluate the reciprocal route the same
way. A 0.5 linear gain on the selected path returns -6.020599913 dB within
1e-5 dB. A mix of two same-phase unity paths from the selected channel returns
6.020599913 dB within 1e-5 dB; adding an inverted unity path cancels to
-300 dB/0 degrees. Mix sums complex path responses in declared edge order
before converting to dB/phase. A single inverted unity path (gain -1 or inverted polarity)
returns 0 dB and exactly -180 degrees, tested with the imaginary part
+0.0 and -0.0. A path whose |H| is 1e-16 returns -300 dB and 0 degrees; one
whose |H| is 1e-14 returns -280 dB and its true phase.
`input_channel=2` is rejected and leaves
output arrays untouched.

### Variable update
`dspg_set_var` never mutates the active graph. It creates a complete candidate
graph from the active graph's retained validated config and protection-record
snapshot, applying one declared variable override within its declared limits.
It reuses the same parser, validator, builder, and protection evaluator as
`dspg_build`. The candidate is unpublished and has fresh node-channel state.
On every failure, candidate output is NULL and the active graph is unchanged.
The application serializes all config mutations (`POST /config` and
`PUT /vars/{name}`) through one transaction gate: while one transaction is
building, persisting, or swapping a candidate, another config mutation returns
`E_BUSY` before building or writing anything. The winning transaction derives
its candidate from the active generation it observed after acquiring that
gate. It writes and reads back the inactive A/B slot and checks its CRC before
requesting the owner-acknowledged graph swap. Only after the owner acknowledges
the swap does the application publish the new active-slot selector and report
success. If the owner rejects the swap, the old graph remains active and the
selector remains on the old slot; the staged inactive slot is ignored at boot.
A selector-write failure after an acknowledged swap is a storage fault: mute
and enter the fail-closed fault path; do not report success. The response is a
non-2xx error, the new graph is live but muted by the fault path, and the
selector still names the old slot, so recovery after review boots the previous
config and ignores the verified staged slot. The owner must acknowledge the
swap within 250 ms (planning value, requires human review). On timeout the
swap state is unknown because the owner may still complete it, so the
application treats it as a fault exactly like a storage fault: mute, enter the
fail-closed fault path, never publish the selector, do not report success. The
transaction gate is released in every outcome (success, rejection, storage
fault, timeout). While the latched fault state is active, config mutations
return `E_BUSY`, so a released gate cannot race an unresolved swap. Power loss
before the selector update boots the previous slot. No network/API path can alter the
protection snapshot. This transaction ordering is a planning contract and
requires human safety review before implementation.

Required transaction vectors: an inactive-slot write, readback, or CRC
failure does not request a graph swap or change the selector; a rejected owner
swap leaves the old graph and selector active and the staged slot is ignored
at boot; when one config mutation holds the gate, a concurrent POST or PUT
returns `E_BUSY` and leaves graph, candidate state, slot contents, and selector
unchanged; a selector-write failure after swap acknowledgment enters the muted
fault path, the request does not report success, the selector keeps naming
the old slot, and a reboot after review selects it; an owner acknowledgment
that does not arrive within 250 ms enters the same muted fault path, the
selector is never written, and the gate is released; while the fault is
latched, a POST or PUT returns `E_BUSY` and builds and writes nothing.

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
int dspg_eval(const dspg_t *g, const char *out_id, uint8_t input_channel,
              const double *frequency_hz, size_t n_frequencies,
              double *magnitude_db, double *phase_deg,
              dspg_report_t *report);
int dspg_get_stats(const dspg_t *g, dspg_stats_t *stats);
```
`input_channel` is exactly 0 or 1 and selects the unit stimulus described
above. These signatures are normative. `dspg_protect_t` is declared in the public
header and is not duplicated in an internal header. `dspg_build` remains as
specified in ADR 0007. `dspg_set_var` and `dspg_eval` return 0 on success and
-1 on failure. `dspg_get_stats` is called only by the DSP owner and returns
0 on success, -1 for null input.

## Verification
Planning contract only. No implementation or response vectors verified.

