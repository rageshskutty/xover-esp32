# Requirements

## Product
- Chain-per-output v1 active crossover, initially 2.1, fixed 48 kHz, ESP32-S3 and I2S TDM output.
- Device JSON declares input channels, optional input user/room-EQ pre-chains, per-output mixes, per-output parametric filter stacks, and one required abstract protection object per output.
- JSON is the source of truth for filter intent. Firmware calculates normalized biquad coefficients in double precision once during complete configuration load; the audio path only executes prepared sections.
- configs/schema.json is the only source of truth for device JSON fields, enums, types, bounds, and section counts. Task cards reference or generate excerpts from it.

## Filter stack and trust boundary
- Filter object fields: type, f_hz, q, gain_db. Supported types: lp1, hp1, lp2, hp2, peq, lshelf, hshelf, ap1, ap2, notch.
- Maximum eight filters per input pre-chain and per output chain; maximum four filters in protection excursion weighting.
- Fixed v1 fs_hz = 48000; filter frequency satisfies 0 < f_hz < 0.45*fs. Any sample-rate mismatch rejects the complete candidate.
- q is 0.1..20 and abs(gain_db) <= 24 dB; type-specific neutral values are fixed by the schema. No filter macros in v1. LR4 is two lp2 or hp2 entries at Q=0.7071.
- Firmware performs coefficient calculation in double precision only at complete configuration load. It validates calculated coefficients and prepares the runtime representation; the real-time path contains no filter-design calculations.
- The output chain is read-only at runtime and replaced only by a complete validated configuration load with CRC/integrity checks and safe publication.
- No driver T/S parameters, driver curves, amplifier/PSU model fields, volts, or watts in firmware, device JSON, NVS configuration, network APIs, or firmware validator inputs. Specifically forbidden: Fs, Qts, Vas, Re, Le, Xmax, Pe, Sd, derived driver fields, amplifier clip/rail data, and supply capacity.
- Firmware validates structure, finite values, filter enums/parameters, section counts, calculated coefficient validity/stability, and declared bounds only. It does not certify designer physical correctness or derive physical limits.

## Protection behavior
- Each output protection object contains peak ceiling, parametric excursion detector weighting, excursion limit, RMS limit/time constant, and release time.
- Protection weighting uses the same filter types as the crossover, maximum four sections, detector path only, and its composite magnitude must be <=0 dB everywhere while never underestimating the normalized X_per_V(f) target. The designer tool verifies this numerically; firmware does not perform this physical curve fit.
- All thresholds are dBFS referenced to the output-chain detector tap. The RMS detector must read 0 dBFS for a full-scale sine; do not use uncorrected sine RMS (-3.0103 dBFS) as the reference.
- Fixed firmware constants: 1 ms lookahead/attack, 5 Hz DC blocker, muted boot/reset. They are not device inputs.
- Gain reduction is the minimum attenuation request from raw peak, excursion-weighted peak, and RMS detectors. Gain is constrained to [0,1] and ramped under T18.
- Limiter is mandatory and last, after all runtime EQ and output processing. Missing/invalid protection leaves output muted.
- Designer-side static margin report covers the base crossover/output chain only; runtime user/room-EQ boosts are covered by the final runtime limiter, not by that report.

## Designer-side protection calculator (never firmware/device JSON)
- Driver inputs: Xmax (one-way linear mm), Pe (W RMS), Z_nom/Z_min (ohms), X_per_V(f) (mm/V peak), voice-coil thermal tau (s).
- Amp/PSU inputs: V_clip_pk (V), rail under load, rated power, PSU rating and efficiency.
- Gain calibration: V0_rms, full-scale sine RMS at speaker terminals, measured using a -20 dBFS 1 kHz sine into a dummy load and multiplying the measured RMS voltage by 10.
- Margins: suggested starting values M_clip=1 dB, M_th=3 dB, M_x=1..2 dB. These are recommendations, not fixed firmware values.
- peak_ceiling_dbfs = min(0, 20*log10(V_clip_pk / V0_pk) - M_clip), with V0_pk=sqrt(2)*V0_rms.
- V_rms_max=sqrt(Pe*Z_min); rms_limit_dbfs=min(0,20*log10(V_rms_max/V0_rms)-M_th).
- RMS detector calibration: full-scale sine reads 0 dBFS.
- V_allow_min=0.9*Xmax/max(X_per_V); excursion_limit_dbfs=20*log10(V_allow_min/V0_pk)-M_x.
- W_target(f)=X_per_V(f)/max(X_per_V(f)); W_target <= 1. Fit with <=4 filters such that abs(W_fit)>=W_target and abs(W_fit)<=1 on the numerical verification grid.
- release_ms starting estimate = clamp((5..10)*1000/f_low_hz,20,5000); 20 Hz sub approximately 250..500 ms.
- rms_tau_ms starting estimate approximately 2000 ms only as general guidance; replace with datasheet or measured thermal time constant.
- Designer-side shared supply check: sum(V_rms_max_i^2/Z_min_i) <= PSU_rating*efficiency. No shared-supply field exists in firmware.
- Worked synthetic sub test vector: Pe=300 W, Z_min=4 ohm, V0_rms=40 V, V_clip_pk=50 V, Xmax=12 mm, max(X_per_V)=0.8 mm/V, M_x=0; expected peak_ceiling=-2.1 dBFS, rms_limit=-4.3 dBFS, excursion_limit=-12.5 dBFS.

## Numeric acceptance and risks
- Proposed host firmware filter-stack response versus SciPy oracle: ±0.05 dB and ±0.5 degree over 20 Hz..20 kHz, pending measured float32 error and human approval.
- Proposed runtime executor: float32 transposed direct form II; compare low-frequency/high-Q cascades against a double-precision reference. If it fails, evaluate 32-bit fixed point with 64-bit accumulators separately.
- Delay resolution at 48 kHz is 20.8333 μs/sample (~7.15 mm); report nearest-sample rounding error.
- Manual VituixCAD-to-JSON re-entry can introduce errors and filter definitions may differ; designer UI plots the generated response for visual overlay/comparison. No VituixCAD coefficient importer is assumed.

## Configuration lifecycle and safety
- Validate and fully prepare a complete candidate before publishing or writing it to the inactive A/B slot.
- Slot header, length, and CRC must pass; candidate JSON must pass current schema and semantic validation.
- If selected slot fails, use the other slot only if it independently passes every check. If neither passes, remain muted.
- Hardware mute, DC blocking, and suitable speaker fuse/PTC remain independent safety layers. Firmware limiting is not the only protection.

## Human gates
- Approve filter formulas and conventions, particularly shelf Q semantics and response comparison against VituixCAD.
- Approve designer protection formulas, RMS reference, margins, excursion envelope fit and physical input quality.
- Approve measured float32 response tolerance or a separate fixed-point evaluation.
- Approve delay maximum/rounding policy.
- Select exact ESP32-S3 module and approve memory budget.
- Approve ADR 0014 and the schema.
- No hardware verification is claimed.
