# ADR 0010: Pure per-output protection comparator contract

- Status: Proposed planning contract; human safety review and CODEOWNERS approval required.
- Date: 2026-11-21
- Depends on: ADR 0008, ADR 0012, and ADR 0013

## Purpose

Replace the former graph-response comparator contract with a pure comparator
for chain-per-output response data. The comparator does not traverse a graph,
parse JSON, design filters, read NVS, access hardware, or apply runtime
limiting. It receives already validated response magnitudes, mix weights,
frequencies, and provisioned limit-curve values.

## Required comparison

For each output (o) and frequency (f_k), the caller supplies one
non-negative finite magnitude (|H_{o,i}(f_k)|) for every input channel
contributing to that output, and the corresponding finite mix weight (w_i).

The comparator calculates:

```text
A_bound,o(f_k) = sum_i(abs(w_i) * abs(H_o,i(f_k)))
```

It converts this conservative amplitude bound to dB and compares it with
the provisioned output limit curve at that same frequency. Exact threshold
equality passes. Any value above the limit fails. No phase cancellation is
credited.

## Input validation

Before making any comparison, validate the complete input set:
- all pointers and pointer/length pairs;
- at least one output, one input contribution per output, and one frequency;
- output IDs are non-empty, NUL-terminated within 32 bytes, unique, and have
  exactly one provisioned curve;
- every curve names exactly one configured output; no unknown or duplicate IDs;
- frequency values are finite and strictly increasing;
- mix weights are finite;
- response magnitudes are finite and non-negative;
- limit-curve values are finite and cover the complete evaluation grid;
- all dimensions agree and remain within the frozen implementation bounds.

Any malformed input returns an error and zeroes the entire non-null failure
record. Malformed input is not a protection-limit violation and must not
report a fabricated output or frequency.

After all inputs validate, compare in configured output order and ascending
frequency order. The first valid limit violation reports output ID,
frequency, calculated conservative bound in dB, and the applicable limit in
dB. A successful call zeroes the failure record.

## Proposed C seam (to be frozen by the comparator prototype)

The exact C structs and function signature are intentionally left for the
Step 4 comparator prototype. That prototype must represent:
- a frequency vector;
- per-output, per-input-channel response-magnitude rows;
- the mix weights for those rows;
- the per-output limit curve sampled on the same vector;
- a deterministic failure record containing output ID, frequency,
  calculated bound, and limit.

Do not preserve or implement the former `dspg_protect_check` signature by
mechanically renaming graph types. Step 4 must add exact bounds, layouts,
pointer rules, and test vectors before implementation work is queued.

## Required test vectors

The prototype must include:
- one-input unity transfer exactly at the limit: pass;
- one-input transfer just above the limit: fail;
- two same-phase unity inputs with weights 0.5 and 0.5: conservative bound
  equals unity;
- two unity inputs with weights 1.0 and 1.0: conservative bound equals 2.0
  (approximately +6.020599913 dB), regardless of phase;
- negative and positive mix weights: magnitude of each weight is used;
- zero transfer: compare using a documented finite floor without NaN/Inf;
- duplicate/missing/unknown output IDs and curve IDs: malformed input;
- NaN/Inf weights, response magnitudes, frequencies, and curve values:
  malformed input;
- descending or duplicate frequencies: malformed input;
- exact limit equality and a just-over-limit value;
- deterministic first-failure ordering;
- failure record zeroing for every malformed input.

The prototype must also prove that a missing curve knot or an attempted
out-of-range interpolation cannot silently disable protection.

## Verification status

Contract proposal only. No comparator implementation or tests have been run.
