# Designer-side tooling

This directory contains host-side tools only; none is linked into firmware.

- protection_oracle.py: independent NumPy/SciPy response oracle. Retain it for designer-side base-chain margin reports and as the double-precision numerical reference for parametric coefficient calculation, runtime biquad response, and limiter math. Current implementation evaluates static chain response against supplied curves; limiter-specific vectors/tests remain future work. User/room-EQ pre-chain boosts are outside the static margin report.
- test_protection_oracle.py and protection_oracle_vectors.json: host reference tests and synthetic data, not driver data or firmware inputs.
- test_scipy_compare.py: cross-checks the existing C SOS/biquad response primitive against SciPy.
- check.py: contains legacy host-only semantic checks for the retired protected-record model. Its driver/amp fields and fixtures are not part of device JSON or firmware input. It is pending separate host-tool cleanup.
- coverage_gate.py: host coverage gate.

The designer-side protection calculator is specified in docs/designer-side/protection-calculator.md. It consumes driver, amplifier, PSU and measured gain data, then emits abstract protection settings and a parametric detector-weighting stack. All physical inputs remain designer-side.

No VituixCAD coefficient importer is assumed. The designer re-enters filter type/frequency/Q/gain into JSON or the UI. The UI plots the calculated response for visual overlay with VituixCAD because filter definitions and digital transforms may differ.

The normative device JSON contract lives only in configs/schema.json. Designer cards and tooling should consume or generate from that source rather than duplicating ranges.
