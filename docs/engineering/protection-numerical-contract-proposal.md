# Protection numerical contract proposal (pre-Step-4 review)

**Status:** proposal only; not approved for implementation.
**Scope:** deterministic numerical conventions for the chain-v1 static protection comparator and its independent NumPy/SciPy reference.

## Fixed behavior already stated by ADR 0008/0010

- For each output and frequency, calculate the conservative amplitude bound:
  `A_bound(f) = Σ abs(weight_i) * abs(H_i(f))`.
- Do not credit phase cancellation between inputs.
- Compare in configured output order, then ascending frequency order.
- Exact threshold equality passes; only a strictly greater calculated bound violates.
- Malformed input is an error, not a limit violation; clear the complete non-null failure record.
- No curve extrapolation is permitted. Every evaluated frequency must be covered by its provisioned curve.
- The comparator receives validated response magnitudes and curves. It does not derive driver, amplifier, supply, or limiter limits.

## Proposed numerical conventions (not yet approved)

| Item | Proposal | Reason / effect |
|---|---|---|
| Arithmetic | IEEE-754 binary64 for host/reference and core-0 comparison | Deterministic reference arithmetic; not real-time processing |
| Zero transfer | finite floor of -200 dB | Avoids log(0), NaN, and infinity |
| Curve interpolation | linear dB against log10(frequency) | Frequency-axis interpolation is logarithmic while limit values remain in dB |
| Out-of-range curve access | reject complete candidate; no extrapolation | A missing curve region must never weaken protection |
| Violation order | output array order, then ascending grid index | Stable first-failure report |
| Equality | equality passes; strictly greater fails | Matches ADR 0010 |

These are review proposals. The reference oracle currently implements them so vectors can be generated and reviewed; their implementation in C is not authorized until approval.

## Evaluation grid

The planning baseline is 1054 logarithmic points from 10 Hz to 20 kHz, plus exact provisioned curve knots and any required exact model boundaries. Exact duplicates are removed and the final grid is strictly increasing.

**Not frozen:** maximum combined point count, maximum per-output input count beyond schema limits, and the total memory budget for all response rows, curves, frequencies, and failure data. The maximum cannot be selected responsibly without a target memory allocation/budget and an agreed storage strategy. It is a hard pre-Step-4 gate.

## Independent reference oracle

- `tools/protection_oracle.py` evaluates supported chain-v1 static blocks using NumPy/SciPy, including input pre-chain, output chain, delay/polarity/gain, designed biquads, and explicit SOS.
- It computes each input-channel transfer independently, then applies the conservative absolute-weight sum.
- It interpolates already provisioned curves using the proposal above and fails closed outside their coverage.
- `tools/protection_oracle_vectors.json` contains comparator vectors; `tools/test_protection_oracle.py` checks malformed inputs, interpolation, no extrapolation, deterministic ordering, and config-to-response calculations.
- This oracle does not derive protection curves from driver/amplifier/supply data. Synthetic curves are test inputs only, not real hardware limits.
- The separate `tools/test_scipy_compare.py` remains the C DSP primitive cross-check; it is not a substitute for this chain-level oracle.

## Remaining pre-Step-4 gates

1. Human safety review and CODEOWNERS approval of ADRs 0008, 0010, 0012, and 0013.
2. Approve or reject the proposed numeric conventions above.
3. Supply a target memory budget and freeze the maximum combined grid size and storage layout.
4. Freeze the exact public C structs, pointer/length rules, maximum dimensions, and failure record in a reviewed header/ADR.
5. Provide owner-approved protected-block derivation and provisioning evidence. No agent may derive real driver/amplifier/supply limits from guessed values.
6. Only after all gates pass, author small Step-4 comparator implementation cards from the approved contract and oracle vectors.

Until then, Step 4 remains blocked. A passing host oracle test proves only the reference implementation and synthetic vectors, not hardware safety.
