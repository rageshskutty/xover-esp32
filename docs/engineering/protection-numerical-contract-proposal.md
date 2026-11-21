# Designer-side protection margin report and SciPy oracle

Status: informative scope note, not a firmware comparator specification.

## Scope
The production C response comparator/API is retired from firmware. Keep the NumPy/SciPy implementation as designer-side tooling to report per-output response margins from designer-supplied transfer and abstract limit data, and as an independent numerical reference for C biquad and limiter math tests. It is not linked into firmware and does not derive driver, amplifier, PSU, thermal, or excursion limits.

The current implementation evaluates static chain response against supplied curves. It does not yet provide limiter-specific vectors/tests; do not claim that capability already exists.

## Designer-side margin convention
For output input weights w_i and complex transfer functions H_i(f), use the conservative magnitude bound:

A_bound(f) = Σ abs(w_i) * abs(H_i(f))

Do not credit phase cancellation. Compare the bound with designer-supplied abstract limits and report margin by output/frequency. This is an engineering report, not a firmware gate or proof of physical safety.

## Firmware authority
The only firmware protection input is the required per-output abstract block defined in configs/schema.json. Firmware checks structure and bounds only. Driver T/S, amplifier/PSU facts, voltage, power, and driver curves stay in designer tooling.

## Human review
Numerical conventions, exact ESP32-S3 module and memory budget, and ADR 0014 approval remain open. No hardware verification is claimed.
