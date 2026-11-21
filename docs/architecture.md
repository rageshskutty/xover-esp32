# Architecture

## Signal path

Input channels → optional per-input user/room-EQ SOS pre-chain → per-output input mix → imported per-output SOS crossover → output gain → polarity → integer-sample delay → fixed 5 Hz DC blocker and detector taps → mandatory attenuation-only limiter → system mute/ramp → I2S TDM output.

## Filter execution contract

Firmware contains no filter-design code. All filters use normalized SOS coefficients b0,b1,b2,a1,a2 with a0=1 and denominator 1+a1 z^-1+a2 z^-2. The same SOS executor is used for input pre-chain EQ, output crossover, and detector-only excursion weighting. V1 runs only at 48 kHz; fs_hz mismatch is a hard configuration failure.

Each output chain is designer-imported and read-only during runtime. It contains at most eight SOS sections, gain, polarity, and integer delay samples. It is replaced only as part of a complete validated configuration load with integrity/CRC verification and safe publication. No partial coefficient or filter mutation API exists.

SOS coefficients must be finite and bounded. The semantic validator rejects any section not satisfying abs(a2)<1 and abs(a1)<1+a2. No filter design, coefficient repair, or runtime sample-rate conversion is performed.

## Protection boundary

Each output carries one required abstract protection object as defined by configs/schema.json. Protection excursion weighting is an SOS cascade (maximum four sections) and affects only the detector path. No separate protected driver record exists in firmware. Driver T/S data, VituixCAD excursion/thermal models, amplifier gain/clip/rail data, and PSU models are designer-side only.

Firmware checks structure, finite values, enumerations, bounds, and SOS stability. It does not evaluate driver curves, infer physical limits, design filters, or decide whether designer-exported limits are safe. Unknown fields reject. Missing or invalid protection keeps the output muted.

## Runtime protection

Raw peak, excursion-weighted peak, and RMS detectors produce independent attenuation requests. Applied gain is their minimum, clamped to [0,1]. Lookahead and attack are fixed at 1 ms; the DC blocker is fixed at 5 Hz; release is per-output configuration. Gain ramps reuse T18. No bypass field exists. Boot/reset is muted and no output is enabled until every output has a valid block.

The limiter is the last audio-processing stage and therefore covers boosts introduced by the runtime input pre-chain EQ. The designer-side static margin report covers only the imported base crossover/output chain; it does not claim to include arbitrary user/room-EQ boosts.

## Storage and failure behavior

A/B slots are independently checked for header, version, length, CRC, parse, schema, and semantic validity. Prefer the selected active slot if fully valid; otherwise fall back only to the other fully valid slot. If neither is valid, remain muted. Candidate updates write and verify the inactive slot before changing the active selector. Never sacrifice the last known-good slot.

## Numeric risks and status

Low-frequency/high-Q SOS sections at 48 kHz require float32 DF2T comparison against a double-precision reference. Proposed response tolerance is ±0.1 dB and ±1 degree over 20 Hz–20 kHz, pending measured float32 error and human approval. Fixed-point with 64-bit accumulators is a fallback to evaluate only if float32 fails.

VituixCAD exports delay in microseconds; designer tooling rounds to integer samples and reports signed time/distance error. The device schema carries samples only.

This is a proposed contract, not an implementation claim. No hardware verification or bench measurement is claimed. ADR 0014 and configs/schema.json are normative.
