# Requirements

## Product
- Chain-per-output v1 active crossover, initially 2.1, fixed 48 kHz, ESP32-S3 and I2S TDM output.
- Device JSON declares input channels, optional input user/room-EQ pre-chains, per-output mixes, imported per-output SOS crossover chains, and one required abstract protection object per output.
- VituixCAD/designer-side tools generate/import the SOS coefficients and abstract protection values; firmware only validates and executes them.
- configs/schema.json is the only source of truth for device JSON. Task cards must reference or generate excerpts from it, never restate ranges.

## Filter execution and trust boundary
- Firmware contains no filter-design code. All filters use SOS coefficients b0,b1,b2,a1,a2, normalized to a0=1, denominator 1+a1 z^-1+a2 z^-2.
- The same SOS executor handles input pre-chain EQ, output crossover, and detector-only excursion weighting.
- Root fs_hz and firmware sample rate are fixed at 48000 Hz. Any mismatch rejects the complete candidate; no runtime redesign or resampling.
- Every output chain has at most eight SOS sections, gain_db, polarity (+1 or -1), and integer delay_samples in 0..4095.
- Input pre-chain user/room EQ uses SOS sections (max eight per channel). Protection excursion weighting uses SOS sections (max four per output).
- The output crossover chain is read-only at runtime and replaced only by a complete validated configuration load with CRC/integrity checks and safe publication. No partial chain mutation API.
- SOS values must be finite and bounded; semantic validation requires abs(a2)<1 and abs(a1)<1+a2.
- No driver T/S parameters, driver curves, amplifier/PSU model fields, volts, or watts in firmware, device JSON, NVS configuration, network APIs, or firmware validator inputs. Specifically forbidden: Fs, Qts, Vas, Re, Le, Xmax, Pe, Sd, derived driver fields, amplifier clip/rail data, and supply capacity.
- Firmware validates structure, finite values, coefficient stability, and declared bounds only. It does not certify designer correctness or derive physical limits.

## VituixCAD import
- VituixCAD 3.0 documents digital biquad export and active-buffer gain, delay, and polarity export.
- The importer must select a declared 48 kHz DSP profile and canonical coefficient sign convention; unknown profiles/sign conventions reject.
- FIR, passive, transfer-function-file, bypassed, and unsupported constructs are rejected or explicitly reported as unrepresented; no silent approximation.
- The VituixCAD active-buffer parameter is in microseconds, while its Buffers export/template reports already-rounded samples. The importer pins the exact path, avoids double rounding, and reports signed time and equivalent-distance rounding error.
- Confirm the actual target VituixCAD version and a golden exported project before freezing the schema/importer.

## Protection behavior
- Each output protection object contains peak ceiling, SOS excursion detector weighting and limit, RMS limit and time constant, and release time.
- Thresholds are dBFS referenced to the output-chain detector tap after imported chain processing.
- Fixed firmware constants: 1 ms lookahead/attack, 5 Hz DC blocker, muted boot/reset. They are not device inputs.
- Gain reduction is the minimum attenuation request from raw peak, excursion-weighted peak, and RMS detectors. Gain is constrained to [0,1] and ramped under T18.
- Limiter is mandatory and last, after all runtime EQ and output processing. Missing/invalid protection leaves output muted.
- Designer-side static margin report covers the base imported crossover/output chain only; runtime user/room-EQ boosts are covered by the final runtime limiter, not by that report.

## Numeric acceptance and open risks
- Proposed host-build response tolerance versus designer-exported response: ±0.1 dB and ±1 degree over 20 Hz–20 kHz. This remains a proposal pending measured float32 error and human approval.
- Proposed executor: float32 transposed direct form II; compare against double precision for low-frequency/high-Q sections. If it fails approved tolerance, evaluate 32-bit fixed point with 64-bit accumulators separately.
- Delay resolution at 48 kHz is 20.8333 μs/sample (~7.15 mm); report nearest-sample rounding error.

## Configuration lifecycle and safety
- Validate a complete candidate before publishing or writing it to the inactive A/B slot.
- Slot header, length, and CRC must pass; candidate JSON must pass current schema and semantic validation.
- If selected slot fails, use the other slot only if it independently passes every check. If neither passes, remain muted.
- Hardware mute, DC blocking, and suitable speaker fuse/PTC remain independent safety layers. Firmware limiting is not the only protection.

## Human gates
- Confirm VituixCAD export version/profile, coefficient convention, 48 kHz sample rate, and a golden export.
- Approve coefficient bounds/stability checks, dBFS/detector conventions, and excursion-weight normalization.
- Approve measured float32 response tolerance or a separate fixed-point evaluation.
- Approve delay maximum and rounding policy.
- Select exact ESP32-S3 module and approve memory budget.
- Approve ADR 0014 and the schema.
- No hardware verification is claimed.
