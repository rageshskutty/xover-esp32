# Designer-side filter-stack response oracle and protection margin report

Status: informative scope note, not a firmware comparator specification.

## Filter-stack numerical reference
JSON stores parametric filter objects. Firmware expands the ordered stack into normalized biquads in double precision at complete configuration load; the audio path executes prepared sections only. ADR 0014 defines the exact formulas and canonical neutral fields for all supported filter types.

The NumPy/SciPy oracle remains designer-side and is the independent double-precision response reference for the load-time coefficient calculator and C runtime biquad response. Proposed response acceptance for host firmware generated from stacked JSON is ±0.05 dB and ±0.5 degree over 20 Hz–20 kHz, pending measured float32 error and human approval. Proposed runtime arithmetic is float32 transposed direct form II; compare low-frequency/high-Q cascades and long runs against double precision. If it fails, evaluate 32-bit fixed point with 64-bit accumulators separately.

## Designer-side margin convention
For output input weights w_i and complex transfer functions H_i(f), use the conservative magnitude bound:

A_bound(f) = Σ abs(w_i) * abs(H_i(f))

Do not credit phase cancellation. Compare the base crossover/output-chain bound with designer-supplied abstract limits and report margin by output/frequency. Do not represent the report as including arbitrary runtime user/room-EQ pre-chain boosts; runtime limiter coverage is responsible for those.

## Excursion-weighting fit
The designer-side tool fits at most four supported parametric filters to W_target(f)=X_per_V(f)/max(X_per_V(f)). On its numerical verification grid, the composite weighting must satisfy abs(W_fit(f)) >= W_target(f) and abs(W_fit(f)) <= 1. Report worst excess and its frequency; reject any fit that underestimates the curve or exceeds 0 dB. The fit is magnitude-only; phase uncertainty remains a physical-model limitation and is addressed by the designer-selected M_x margin.

## Protection calculator
All physical inputs and formulas are in docs/designer-side/protection-calculator.md. This is designer-only tooling; no driver T/S, curve, voltage, wattage, amp or PSU model data enters device JSON or firmware.

## Firmware authority
The only firmware protection input is the required per-output abstract block defined in configs/schema.json. Firmware checks structure, parameter bounds and calculated coefficient validity/stability only. It does not derive driver, amplifier, PSU, thermal or excursion limits.

## Status
No firmware implementation, hardware measurement, or bench verification is claimed.
