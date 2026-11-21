# Requirements

## Product
- Chain-per-output v1 active crossover, initially 2.1, 48 kHz, ESP32-S3 and I2S TDM output.
- Device JSON declares inputs, optional input pre-chains, per-output mixes, ordered user chains, and one required abstract protection object per output.
- VituixCAD and designer-side tools generate crossover, EQ, and abstract protection values; firmware validates and executes them.
- configs/schema.json is the only source of truth for device JSON. Task cards must reference or generate excerpts from it, never restate ranges.

## Firmware/device trust boundary
- No driver T/S parameters, driver curves, amplifier/PSU model fields, volts, or watts in firmware, device JSON, NVS configuration, network APIs, or firmware validator inputs.
- Specifically forbidden: Fs, Qts, Vas, Re, Le, Xmax, Pe, Sd, derived driver fields, amplifier clip/rail data, and supply capacity.
- Designer tools may consume those source facts and export only the abstract per-output fields defined by the schema.
- Firmware validates structure, finite numeric values, and declared bounds only. It does not certify designer correctness or derive physical limits.
- Unknown fields reject. Every output requires its protection object. There is no bypass property.

## Protection behavior
- Each output protection object contains peak ceiling, excursion detector weighting and limit, RMS limit and time constant, and release time.
- Thresholds are dBFS referenced to the output-chain end, after crossover and user processing.
- Excursion weighting is a detector-only cascade of 0–4 biquad specifications using the schema enum.
- Fixed firmware constants: 1 ms lookahead/attack, 5 Hz DC blocker, muted boot/reset. They are not device inputs.
- Gain reduction is the minimum attenuation request from raw peak, excursion-weighted peak, and RMS detectors. Gain is constrained to [0,1] and ramped under T18.
- Limiter is mandatory and last, after user PEQ and gain. Missing/invalid protection leaves output muted.

## Configuration lifecycle and safety
- Validate a complete candidate before publishing or writing it to the inactive A/B slot.
- Slot header, length, and CRC must pass; candidate JSON must pass current schema and semantic validation.
- If selected slot fails, use the other slot only if it independently passes every check. If neither passes, remain muted.
- Hardware mute, DC blocking, and suitable speaker fuse/PTC remain independent safety layers. Firmware limiting is not the only protection.

## Human gates
- Approve numerical conventions and detector/weighting normalization.
- Select exact ESP32-S3 module and approve memory budget.
- Approve ADR 0014 and the schema.
- No hardware verification is claimed.
