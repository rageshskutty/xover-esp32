# Designer-side tooling

This directory contains host-side tools only; none is linked into firmware.

- protection_oracle.py: independent NumPy/SciPy response oracle. Retain it for designer-side per-output margin reports and as the numerical reference for C biquad and limiter math tests. Current implementation evaluates static chain response against supplied curves; limiter-specific vectors/tests remain future work.
- test_protection_oracle.py and protection_oracle_vectors.json: host reference tests and synthetic data, not driver data or firmware inputs.
- test_scipy_compare.py: cross-checks the existing C SOS/biquad response primitive against SciPy.
- check.py: currently contains legacy host-only semantic checks for the retired protected-record model. Its driver/amp fields and fixtures are not part of device JSON or firmware input. Remove that legacy branch during later tooling cleanup; do not use it to define the new contract.
- coverage_gate.py: host coverage gate.

The normative device JSON contract lives only in configs/schema.json. Designer cards and tooling should consume or generate from that source rather than duplicating ranges.
