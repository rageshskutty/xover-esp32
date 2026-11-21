# Contracts and verification

## Authority
- ADR 0012: chain-per-output architecture.
- ADR 0014: imported SOS execution and abstract per-output protection contract.
- configs/schema.json: sole source of truth for device JSON fields, coefficient bounds, section counts, and protection ranges.
- Existing DSP headers: implemented primitive interfaces only.

ADRs 0008, 0010, and 0013 are superseded for firmware protection. The production C response comparator and T/S-derived firmware gates are retired.

## Filter execution
Firmware contains no filter-design code. Input pre-chain EQ, output crossover, and protection weighting all use the same normalized SOS representation and executor. V1 is fixed at 48 kHz. SOS stability is checked semantically with abs(a2)<1 and abs(a1)<1+a2. Output chain is immutable during normal runtime and changes only through complete validated configuration replacement.

VituixCAD 3.0 documents digital biquad export and active-buffer gain/delay/polarity export. The import path must pin a declared 48 kHz DSP profile and coefficient sign convention. VituixCAD's active-buffer parameter is in microseconds, but the Buffers export/template reports already-rounded samples. The importer pins the exact path, avoids double rounding, and reports rounding error. Golden export verification remains open.

## Numeric verification
Proposed host-build response acceptance versus the designer export is ±0.1 dB and ±1 degree from 20 Hz to 20 kHz. This is not approved until measured float32 error is known. Proposed float32 executor is transposed direct form II; compare low-frequency/high-Q sections and cascades against double precision. Fixed point with 64-bit accumulators is an evaluation fallback if float32 misses approved limits.

## SciPy oracle
Keep tools/protection_oracle.py and its tests. It is designer-side base-chain margin reporting and an independent double-precision reference for C SOS and limiter math. It is not linked into firmware and does not derive driver limits. Current implementation evaluates static chain response against supplied curves; limiter-specific vectors are future work, not an existing capability. Margin report does not include arbitrary runtime user/room-EQ boosts.

## Verification status
This is documentation/schema work only. No firmware code was changed, no local test suite or external JSON Schema validator was run, and no hardware or bench verification was performed. CI status must be checked on the PR head before implementation work.
