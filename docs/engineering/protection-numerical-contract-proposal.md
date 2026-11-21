# Designer-side protection margin report and SOS numerical oracle

Status: informative scope note, not a firmware comparator specification.

## Scope
The production C response comparator/API is retired from firmware. Keep the NumPy/SciPy implementation as designer-side base-chain margin reporting and as an independent double-precision numerical reference for C SOS and limiter math tests. It is not linked into firmware and does not derive driver, amplifier, PSU, thermal, or excursion limits.

The current implementation evaluates static chain response against supplied curves. It does not yet provide limiter-specific vectors/tests; do not claim that capability already exists. The margin report covers the imported base crossover/output chain only and excludes arbitrary runtime user/room-EQ pre-chain boosts.

## Canonical SOS convention
All runtime filters use:
H(z)=(b0+b1 z^-1+b2 z^-2)/(1+a1 z^-1+a2 z^-2), with a0=1.
The same executor handles input pre-chain EQ, output crossover, and protection detector weighting. Firmware contains no filter-design code.

Every section is finite and bounded and must satisfy:
- abs(a2) < 1
- abs(a1) < 1 + a2

The proposed firmware executor is float32 transposed direct form II. Compare against double precision for low-frequency/high-Q sections at 48 kHz. Proposed response tolerance is ±0.1 dB and ±1 degree over 20 Hz–20 kHz, pending measured float32 error and human approval. If float32 fails, evaluate 32-bit fixed point with 64-bit accumulators as a separate design decision.

## Designer-side margin convention
For output input weights w_i and complex transfer functions H_i(f), use the conservative magnitude bound:

A_bound(f) = Σ abs(w_i) * abs(H_i(f))

Do not credit phase cancellation. Compare the bound with designer-supplied abstract limits and report margin by output/frequency. Do not represent the report as including user/room-EQ boosts; runtime limiter coverage is responsible for those.

## VituixCAD import and delay
VituixCAD 3.0 documents digital biquad export and active-buffer gain/delay/polarity export. The active-buffer parameter is in microseconds, but the Copy/Export window's Buffers option and EQ-template delay variable report samples already rounded. Pin the exact import/export path, do not double-round sample values, and verify units against a golden project. If consuming the raw microsecond parameter, convert with round(delay_us * 48000 / 1e6). Report signed time and equivalent-distance error. Confirm the actual target version and a golden exported project before freezing importer behavior.

## Firmware authority
The only firmware protection input is the required per-output abstract block defined in configs/schema.json. Firmware checks structure, bounds, and SOS stability only. Driver T/S, amplifier/PSU facts, voltage, power, and driver curves stay in designer tooling.

## Human review
VituixCAD version/profile and golden export, numerical conventions, coefficient bounds, float32 acceptance, exact ESP32-S3 module and memory budget, and ADR 0014 approval remain open. No hardware verification is claimed.
