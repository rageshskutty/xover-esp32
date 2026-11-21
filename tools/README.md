# Designer-side tooling

This directory contains host-side tools only; none is linked into firmware.

- protection_oracle.py: independent NumPy/SciPy response oracle. Retain it for designer-side base-chain margin reports and as the double-precision numerical reference for C SOS and limiter math tests. Current implementation evaluates static chain response against supplied curves; limiter-specific vectors/tests remain future work. User/room-EQ pre-chain boosts are outside the static margin report.
- test_protection_oracle.py and protection_oracle_vectors.json: host reference tests and synthetic data, not driver data or firmware inputs.
- test_scipy_compare.py: cross-checks the existing C SOS/biquad response primitive against SciPy.
- check.py: currently contains legacy host-only semantic checks for the retired protected-record model. Its driver/amp fields and fixtures are not part of device JSON or firmware input. It is explicitly retired from defining protection semantics and is pending later host-tool cleanup.
- coverage_gate.py: host coverage gate.

VituixCAD import tooling must consume exported normalized SOS coefficients, an explicitly selected 48 kHz DSP profile, and a declared coefficient sign convention. It pins the exact delay source: the active-buffer parameter is in microseconds, while the Buffers export/template reports already-rounded samples. It avoids double rounding and reports signed rounding error. It must reject unsupported FIR/passive/bypassed constructs rather than silently approximating them.

The normative device JSON contract lives only in configs/schema.json. Designer cards and tooling should consume or generate from that source rather than duplicating ranges.
