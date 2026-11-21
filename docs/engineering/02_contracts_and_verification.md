# Contracts and verification

## Authority
- ADR 0012: chain-per-output architecture.
- ADR 0014: abstract per-output protection contract and trust boundary.
- configs/schema.json: sole source of truth for device JSON and protection ranges.
- Existing DSP headers: implemented primitive interfaces only.

ADRs 0008, 0010, and 0013 are superseded for firmware protection. The production C response comparator and T/S-derived firmware gates are retired.

## Firmware boundary
Firmware contains no driver T/S parameters, driver curves, amplifier/PSU model fields, volts, or watts. The designer tool consumes those inputs and exports the abstract per-output block. Firmware validates structure and bounds only; it cannot certify designer correctness.

## SciPy oracle
Keep tools/protection_oracle.py and its tests. It is designer-side tooling for response/margin reporting and an independent numerical reference for C biquad and limiter math tests. It is not linked into firmware and does not derive driver limits. The current oracle code covers static chain response and supplied abstract curves; limiter-specific vectors are future work, not an existing capability.

## Verification status
This is documentation/schema work only. No firmware code was changed, no local tests were run, and no hardware or bench verification was performed. CI status must be checked on the PR head before implementation work.
