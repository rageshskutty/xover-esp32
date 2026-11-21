# Contracts and verification

## Authority
- ADR 0012: chain-per-output architecture.
- ADR 0014: parametric filter-stack execution and abstract per-output protection contract.
- configs/schema.json: sole source of truth for device JSON fields, types, enums, bounds, and section counts.
- Existing DSP headers: implemented primitive interfaces only.

ADRs 0008, 0010, and 0013 are superseded for firmware protection. The production C response comparator and T/S-derived firmware gates are retired.

## Filter-stack execution
JSON stores parametric filters; firmware calculates normalized biquad coefficients in double precision once during complete configuration load. The real-time path executes prepared sections only. Input pre-chain EQ, output crossover, and protection detector weighting use the same filter vocabulary and coefficient calculator. V1 is fixed at 48 kHz. Supported types, formulas, neutral-field rules and bounds are in ADR 0014 and configs/schema.json.

The output chain is immutable during normal runtime and changes only through complete validated configuration replacement. LR4 is explicitly represented as two lp2/hp2 sections at Q=0.7071. There is no VituixCAD coefficient import; the designer UI plots the generated response for visual comparison with VituixCAD.

## Numeric verification
Proposed host firmware response from stacked JSON versus the independent SciPy oracle is ±0.05 dB and ±0.5 degree from 20 Hz to 20 kHz. This is not approved until measured float32 error is known. Proposed float32 executor is transposed direct form II; compare low-frequency/high-Q cascades against double precision. Fixed point with 64-bit accumulators is an evaluation fallback if float32 misses approved limits.

## Protection calculator and oracle
The designer-only calculator maps driver, amp, PSU and measured V0_rms inputs into abstract per-output protection values. Its formulas and worked sub vector are in docs/designer-side/protection-calculator.md. The RMS detector convention is 0 dBFS for a full-scale sine. The excursion-weighting stack must numerically envelope the normalized X_per_V(f) curve without exceeding 0 dB. These physical calculations are not firmware responsibilities.

Keep tools/protection_oracle.py as designer-side base-chain margin reporting and an independent double-precision reference for coefficient calculator, runtime biquad response and limiter math. It is not linked into firmware. Current implementation does not yet provide limiter-specific vectors; do not claim otherwise. Margin reporting excludes arbitrary runtime user/room-EQ boosts.

## Verification status
This is documentation/schema work only. No firmware code was changed, no local test suite or external JSON Schema validator was run, and no hardware or bench verification was performed. CI status must be checked on the PR head before implementation work.
