# Architecture

## Signal path
Input channels → optional per-input user/room-EQ parametric filter pre-chain → per-output input mix → per-output parametric crossover filter stack → output gain → polarity → integer-sample delay → fixed 5 Hz DC blocker and detector taps → mandatory attenuation-only limiter → system mute/ramp → I2S TDM output.

## Filter-stack execution contract
JSON is the source of truth and stores ordered filter objects {type,f_hz,q,gain_db}. Supported types and their exact coefficient convention are defined in ADR 0014. The same load-time coefficient calculator expands input pre-chain, output crossover, and detector-only protection weighting into normalized biquad sections.

Firmware calculates coefficients in double precision once when loading a complete configuration. The real-time audio path executes prepared sections only; no filter design, coefficient calculation, or trigonometric work occurs per sample or per block. V1 is fixed at 48 kHz; fs_hz mismatch is a hard configuration failure.

Each output chain has at most eight filters, gain, polarity, and integer delay samples. Protection weighting has at most four filters. LR4 is two lp2 or hp2 sections at Q=0.7071; v1 has no macros. Filter parameter bounds and type-specific neutral values are normative in configs/schema.json.

The output chain is immutable during normal runtime. It is replaced only by a complete validated configuration load with CRC/integrity verification and safe publication. No partial filter mutation API exists. The load-time calculator rejects non-finite or unstable computed sections; no coefficient repair is allowed.

## Protection boundary
Each output carries one required abstract protection object as defined by configs/schema.json. Its excursion_weighting filter stack uses the same parametric filter vocabulary and affects only the detector path. Its composite magnitude is designer-verified to be <=0 dB and to conservatively envelope the normalized X_per_V(f) curve. Firmware does not calculate driver models or validate physical correctness.

Driver T/S data, X_per_V curves, amplifier gain/clip/rail data, and PSU models are designer-side only. Firmware checks structure, finite values, enumerations, bounds, and computed coefficient stability. Unknown fields reject. Missing or invalid protection keeps the output muted.

## Runtime protection
Raw peak, excursion-weighted peak, and RMS detectors produce independent attenuation requests. The RMS detector is calibrated so a full-scale sine reads 0 dBFS. Applied gain is their minimum, clamped to [0,1]. Lookahead and attack are fixed at 1 ms; the DC blocker is fixed at 5 Hz; release is per-output configuration. Gain ramps reuse T18. No bypass field exists. Boot/reset is muted and no output is enabled until every output has a valid block.

The limiter is the last audio-processing stage and covers boosts introduced by the runtime input pre-chain EQ. The designer-side static margin report covers only the base crossover/output chain; it excludes arbitrary user/room-EQ boosts.

## Storage and failure behavior
A/B slots are independently checked for header, version, length, CRC, parse, schema, parameter, and computed-coefficient validity. Prefer the selected active slot if fully valid; otherwise fall back only to the other fully valid slot. If neither is valid, remain muted. Candidate updates validate and prepare a complete configuration before writing/verifying the inactive slot and changing the active selector. Never sacrifice the last known-good slot.

## Numeric risks and status
Low-frequency/high-Q sections at 48 kHz require float32 DF2T comparison against a double-precision reference. Proposed response tolerance is ±0.05 dB and ±0.5 degree over 20 Hz–20 kHz, pending measured float32 error and human approval. Fixed-point with 64-bit accumulators is a fallback to evaluate only if float32 fails.

Delay resolution is 20.8333 μs/sample at 48 kHz; the designer UI reports nearest-sample rounding error. Manual VituixCAD-to-JSON re-entry can differ from VituixCAD's filter definitions, so the UI plots the generated response for visual overlay.

This is a proposed contract, not an implementation claim. No hardware verification or bench measurement is claimed. ADR 0014 and configs/schema.json are normative.
