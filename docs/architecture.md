# Architecture

## Signal path
Input channels → optional per-input pre-chain → per-output input mix → ordered per-output user DSP chain (crossover, PEQ, gain, delay, polarity) → fixed 5 Hz DC blocker and detector taps → mandatory attenuation-only limiter → system mute/ramp → I2S TDM output.

The limiter is the final audio-processing stage. It runs after every user stage, including PEQ and gain. Excursion weighting affects only its detector path, never programme audio.

## Protection boundary
Each output carries one required abstract protection object as defined by configs/schema.json. No separate protected driver record exists in firmware. Driver T/S data, VituixCAD excursion/thermal models, amplifier gain/clip/rail data, and PSU models are designer-side only. The designer converts them into dBFS limits and detector weighting.

Firmware checks structure, finite values, enumerations, and bounds. It does not evaluate driver curves, infer physical limits, or decide whether designer-exported limits are safe. Unknown fields reject. Missing or invalid protection keeps the output muted.

## Runtime protection
Raw peak, excursion-weighted peak, and RMS detectors produce independent attenuation requests. Applied gain is their minimum, clamped to [0,1]. Lookahead and attack are fixed at 1 ms; the DC blocker is fixed at 5 Hz; release is per-output configuration. Gain ramps reuse T18. No bypass field exists. Boot/reset is muted and no output is enabled until every output has a valid block.

## Storage and failure behavior
A/B slots are independently checked for header, version, length, CRC, parse, schema, and semantic validity. Prefer the selected active slot if fully valid; otherwise fall back only to the other fully valid slot. If neither is valid, remain muted. Candidate updates write and verify the inactive slot before changing the active selector. Never sacrifice the last known-good slot.

## Status
This is a proposed contract, not an implementation claim. No hardware verification or bench measurement is claimed. ADR 0014 and configs/schema.json are normative.
