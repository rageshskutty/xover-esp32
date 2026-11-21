# 7B execution catalog

## Contract authority
ADR 0012 defines chain-per-output architecture. ADR 0014 defines parametric filter-stack execution and firmware protection behavior. configs/schema.json is the single source of truth for device JSON fields, types, bounds, section counts, and enums.

Task cards must reference the schema path and relevant JSON Pointer. If a card needs a compact schema excerpt, generate it from configs/schema.json; never restate or independently maintain protection ranges in a prompt. If a card conflicts with the schema or ADR 0014, stop and report.

## Filter execution work (not yet authorized)
After human approval, implementation must:
1. Implement the load-time double-precision coefficient calculator for all ten filter types exactly as ADR 0014 defines; keep it outside the real-time audio path.
2. Validate complete filter stacks, finite parameters, enums, parameter bounds, section counts, fixed 48 kHz rate, and computed coefficient finiteness/stability.
3. Prepare runtime biquad sections and state using the selected float32 DF2T executor; no coefficient calculations in the audio task.
4. Implement gain, polarity, and bounded integer delay as part of the per-output chain.
5. Install complete validated/prepared configurations atomically; no runtime partial crossover mutation.
6. Compare host firmware response from stacked JSON against the SciPy oracle using the proposed tolerance only after measured float32 error is reviewed.

## Protection work (not yet authorized)
After human approval, implementation order is:
1. Validate closed JSON structure, finite values, declared filter bounds, section counts, calculated coefficient validity/stability, and per-output protection.
2. Implement raw peak, detector-only parametric excursion weighting, and calibrated RMS detectors (full-scale sine reads 0 dBFS).
3. Implement mandatory attenuation-only limiter using fixed 1 ms lookahead/attack, 5 Hz DC blocker, per-output release, and T18 ramp behavior.
4. Wire protection as the final stage after all runtime user/output processing.

No firmware implementation is part of the documentation/schema PR. Do not generate a C comparator task. Former Gate 3 (C comparator API) and Gate 5 (sourced driver data) are retired as firmware gates.

## Designer-side calculator and UI
Implement the formulas in docs/designer-side/protection-calculator.md in designer tooling only. Keep all driver, amp, PSU, voltage and power inputs out of device JSON and firmware. Numerically verify the <=4-section excursion weighting envelope and plot the resulting crossover/PEQ/protection responses for visual comparison with VituixCAD. No automatic VituixCAD import is assumed.

## SciPy oracle
Retain the NumPy/SciPy oracle as designer-side base-chain margin reporting and an independent double-precision reference for coefficient calculation and C runtime biquad/limiter math. It is not a firmware runtime dependency. Current static-response implementation does not yet provide limiter-specific vectors. The margin report excludes arbitrary runtime user/room-EQ boosts.

## Hardware gates
Mute-at-boot, DC blocking, amplifier mute, and suitable fuse/PTC remain independent safety layers. Exact ESP32-S3 module, memory budget, and hardware verification remain human-owned.
