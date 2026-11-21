# 7B execution catalog

## Contract authority
ADR 0012 defines chain-per-output architecture. ADR 0014 defines imported SOS execution and firmware protection behavior. configs/schema.json is the single source of truth for device JSON fields, types, bounds, section counts, and enums.

Task cards must reference the schema path and relevant JSON Pointer. If a card needs a compact schema excerpt, generate it from configs/schema.json; never restate or independently maintain protection ranges in a prompt. If a card conflicts with the schema or ADR 0014, stop and report.

## Filter execution work (not yet authorized)
After human approval, implementation must:
1. Implement the single normalized SOS executor (float32 transposed direct form II) for pre-chain, output crossover, and detector weighting. Firmware must not contain filter-design functions.
2. Validate complete imported SOS arrays, finite/bounded coefficients, section counts, fixed 48 kHz rate, and strict stability inequalities.
3. Implement gain, polarity, and bounded integer delay as part of the per-output imported chain.
4. Install complete validated configurations atomically; no runtime partial crossover mutation.
5. Compare host response against designer-exported VituixCAD response and double-precision reference using the proposed tolerance only after measured float32 error is reviewed.

## Protection work (not yet authorized)
After human approval, implementation order is:
1. Validate closed JSON structure, finite values, declared bounds, SOS stability, and per-output protection.
2. Implement raw peak, SOS-weighted peak, and RMS detectors.
3. Implement mandatory attenuation-only limiter using fixed 1 ms lookahead/attack, 5 Hz DC blocker, per-output release, and T18 ramp behavior.
4. Wire protection as the final stage after all runtime user/output processing.

No firmware implementation is part of the documentation/schema PR. Do not generate a C comparator task. Former Gate 3 (C comparator API) and Gate 5 (sourced driver data) are retired as firmware gates.

## VituixCAD import gate
VituixCAD 3.0 help documents digital biquad export plus active-buffer gain, delay, and polarity. The importer must explicitly select the 48 kHz DSP profile and canonical coefficient convention, reject unsupported blocks, and provide a golden exported project/vector. Pin whether delay is read from the active-buffer microsecond parameter or already-rounded Buffers export/template samples; avoid double rounding and report signed time/distance error. Actual importer compatibility remains a human verification gate.

## SciPy oracle
Retain the NumPy/SciPy oracle as designer-side base-chain margin reporting and an independent double-precision reference for C SOS and limiter math. It is not a firmware runtime dependency. Current static-response implementation does not yet provide limiter-specific vectors. The margin report excludes arbitrary runtime user/room-EQ boosts.

## Hardware gates
Mute-at-boot, DC blocking, amplifier mute, and suitable fuse/PTC remain independent safety layers. Exact ESP32-S3 module, memory budget, and hardware verification remain human-owned.
