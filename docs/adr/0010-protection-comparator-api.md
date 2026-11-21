# ADR 0010: Protection evaluator seam and record bounds
- Status: Accepted for planning; human CODEOWNERS review required before implementation
- Date: 2026-11-19
- Depends on: ADR 0008

## Exact pure function seam
```c
typedef struct {
    char out_id[32];
    double frequency_hz;
    double response_db;
} dspg_protect_failure_t;
int dspg_protect_check(const dspg_protect_t *records, size_t n_records,
                       const char *const *output_ids, size_t n_outputs,
                       const double *frequency_hz,
                       const double *const response_db_by_output[],
                       size_t n_frequencies,
                       dspg_protect_failure_t *failure);
```
Return 0 only if all records and every output sample pass; return -1 otherwise.
This API is core-0-only, read-only, allocation-free, and consumes caller-
supplied curves. It does not traverse a graph, construct complex responses,
access hardware, parse JSON or read NVS. Frequencies must be finite and
strictly increasing.

### Input validation and failure contents
Validate all pointers, lengths, frequency values, output IDs, response row
pointers and samples, and all protection records before comparing any output
against a limit. `n_outputs==0` or `n_frequencies==0` is invalid. Empty input
returns -1 with the entire non-null failure structure zeroed. Any non-finite
response sample is invalid, returns -1, and zeroes the entire failure
structure. All malformed input, including malformed records, returns -1 with
`failure` zeroed (empty `out_id`, frequency 0, response 0); do not use NaN as
a sentinel. If `failure==NULL`, return -1 without dereferencing it.

Require a one-to-one ID correspondence: each output ID is non-empty, NUL-
terminated within 32 bytes, unique, and has exactly one record; every record
ID is likewise valid, unique, and names one output. Counts must match.
In particular, outputs `{o,o}` and records `{o,p}` reject. This check must
not be implemented as only a first-match lookup per output.

Once all input validates, compare in supplied output order and ascending
frequency order. A valid finite response-limit violation returns -1 and fills
the first violating `out_id`, `frequency_hz`, and `response_db`. On success,
zero the failure structure. A threshold equality passes; reject only when
`response_db > limit`.

## Record format bounds
`max_gain_db` in [-120,+24] dB; edge zero disables; enabled edges [10,20000]
Hz; attenuation [0,120] dB. These are representational sanity limits only.
Human must approve device limits and record policy before code is accepted.
Lower band applies for `f<=edge`; upper band applies for `f>=edge`.
Disabled-band attenuation must be exactly zero. Starter vectors and 1e-5 dB
tolerance are in ADR 0008.

## Test requirements
Use table-driven tests for max-gain equality/just-over, lower/upper edge
equality, disabled bands, ordering, malformed records, duplicate/missing/
unknown IDs, the equal-count non-bijection vector, NaN/Inf response samples,
non-monotonic frequencies, zero outputs/frequencies and nulls. Assert that
every malformed case returns -1 and zeroes all failure fields. A valid
violation reports its deterministic first output/frequency. Tests remain
unregistered in CMake until a task explicitly registers them.

## Verification
Contract only; no implementation verified.
