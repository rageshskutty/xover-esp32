# 7B execution catalog

## Contract authority
ADR 0012 defines chain-per-output architecture. ADR 0014 defines firmware protection behavior. configs/schema.json is the single source of truth for device JSON, field names, types, enums, and numeric ranges.

Task cards must reference the schema path and relevant JSON Pointer. If a card needs a compact schema excerpt, generate it from configs/schema.json; never restate or independently maintain protection ranges in a prompt. If a card conflicts with the schema or ADR 0014, stop and report.

## Protection work (not yet authorized)
After human approval, implementation order is:
1. Validate closed JSON structure, finite values, and declared bounds.
2. Implement raw peak, excursion-weighted peak, and RMS detectors.
3. Implement mandatory attenuation-only limiter using fixed 1 ms lookahead/attack, 5 Hz DC blocker, per-output release, and T18 ramp behavior.
4. Wire protection as the final stage after the user chain.

No firmware implementation is part of the documentation/schema PR. Do not generate a C comparator task. Former Gate 3 (C comparator API) and Gate 5 (sourced driver data) are retired as firmware gates.

## SciPy oracle
Retain the NumPy/SciPy oracle as designer-side response/margin reporting and an independent reference for C biquad and limiter math tests. It is not a firmware runtime dependency. Current static-response implementation does not yet provide limiter-specific vectors.

## Hardware gates
Mute-at-boot, DC blocking, amplifier mute, and suitable fuse/PTC remain independent safety layers. Exact ESP32-S3 module, memory budget, and hardware verification remain human-owned.
