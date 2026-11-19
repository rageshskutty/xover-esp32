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
Return 0 only if all records and every output sample pass; return -1 otherwise. On failure, fill the first failing output/frequency in deterministic output order then ascending-frequency order; malformed record failure sets frequency_hz=0 and response_db=NaN is forbidden (set 0). Null pointer/length mismatch is failure. This function only validates record correspondence and compares caller-supplied finite dB curves; graph traversal and complex response construction are a separate module. The function does not allocate, access hardware, parse JSON or read NVS. Frequency arrays are strictly increasing and finite; invalid input fails closed.

Record format bounds: max_gain_db in [-120,+24] dB; edge zero disables; enabled edges [10,20000] Hz; attenuation [0,120] dB. These are representational sanity limits only. Human must approve device limits and record policy before code is accepted. Response samples use exact decimal dB expectations; threshold equality passes (reject only response_db > limit).

## Test requirements
Use table-driven tests for max-gain equality/just-over, lower/upper edge equality, disabled bands, ordering, malformed records, duplicate/missing/unknown IDs, NaN/Inf, non-monotonic frequencies and nulls. Starter vectors and 1e-5 dB tolerance are in ADR 0008. Test function writes a zeroed failure struct on success.

## Verification
Contract only; no implementation verified.
