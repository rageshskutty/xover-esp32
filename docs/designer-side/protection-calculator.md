# Designer-side protection calculator

Status: informative designer-tool specification. None of these physical inputs or intermediate values are firmware/device JSON fields.

## Inputs

| Group | Inputs |
|---|---|
| Driver | Xmax (mm, one-way linear), Pe (W RMS), Z_nom and Z_min (ohms), X_per_V(f) (mm/V peak from VituixCAD simulation), voice-coil thermal time constant tau (s) |
| Amplifier and PSU | V_clip_pk (V), rail under load, rated amplifier power, PSU rating, PSU efficiency |
| Gain calibration | V0_rms: full-scale sine RMS voltage at the speaker terminals |
| Margins | M_clip = 1 dB, M_th = 3 dB, M_x = 1–2 dB are suggested starting values; M_x = 0 is used in the worked test vector |

Measure V0_rms by playing a -20 dBFS 1 kHz sine into a suitable dummy load, measuring the speaker-terminal RMS voltage, and multiplying the measured value by 10. Then:

`V0_pk = sqrt(2) * V0_rms`

This calibration procedure and all volts/watts/driver data remain in designer tooling only.

## Amplifier clipping / peak ceiling

`peak_ceiling_dbfs = min(0, 20*log10(V_clip_pk / V0_pk) - M_clip)`

Use the amplifier's peak clipping voltage under the relevant loaded condition. Rail-under-load and rated-power data are designer-side cross-checks; do not serialize them.

## Thermal / RMS limit

`V_rms_max = sqrt(Pe * Z_min)`

`rms_limit_dbfs = min(0, 20*log10(V_rms_max / V0_rms) - M_th)`

The firmware RMS detector must read 0 dBFS for a full-scale sine. Do not define 0 dBFS as the uncorrected RMS of a peak-full-scale sine (which is -3.0103 dB). The suggested initial `rms_tau_ms` is approximately 2000 ms, but this is general guidance only. If using the voice-coil thermal time constant `tau_s` directly, convert with `rms_tau_ms = 1000 * tau_s` and verify that this detector integration behavior is appropriate for the driver's thermal model. Prefer a datasheet value or measured thermal response; neither 2000 ms nor a direct one-to-one mapping is universal.

## Excursion limit and weighting envelope

`V_allow_min = 0.9 * Xmax / max(X_per_V(f))`

`excursion_limit_dbfs = 20*log10(V_allow_min / V0_pk) - M_x`

`W_target(f) = X_per_V(f) / max(X_per_V(f))`

The normalized target is no greater than 1 (0 dB). Fit it with at most four supported parametric filters. On a sufficiently dense numerical verification grid, the composite weighting must satisfy both:

- `abs(W_fit(f)) >= W_target(f)` — the fit must never underestimate the true normalized excursion curve.
- `abs(W_fit(f)) <= 1` — the weighting must never exceed 0 dB.

The designer tool must check this numerically, report worst envelope excess and its frequency, and reject a fit that violates either condition. This is not a firmware runtime calculation.

Magnitude-only weighting ignores phase in the true excursion response; waveform peak error may be around 1–2 dB. M_x is a reserve for this uncertainty, not proof that phase error is bounded to that value. Sustained bass heating may also exceed a sine-based Pe assumption, so M_th matters.

## Release and supply checks

Suggested release estimate:

`release_ms ~= clamp((5..10) * 1000 / f_low_hz, 20, 5000)`

Choose a multiplier between 5 and 10. A 20 Hz subwoofer gives approximately 250–500 ms. This is a starting rule for listening/measurement review, not a mandatory firmware formula.

Designer-side shared-supply check:

`sum_over_outputs(V_rms_max_i^2 / Z_min_i) <= PSU_rating * efficiency`

Use the same conservative impedance basis consistently for each output. This check replaces any shared-supply field in firmware. Rail sag, amplifier rated power, and PSU continuous/thermal rating remain designer-side cross-checks.

## Worked subwoofer test vector

Inputs: Pe = 300 W, Z_min = 4 ohms, V0_rms = 40 V, V_clip_pk = 50 V, Xmax = 12 mm, max(X_per_V) = 0.8 mm/V, M_clip = 1 dB, M_th = 3 dB, M_x = 0 dB.

- V0_pk = sqrt(2)*40 = 56.5685 V peak.
- peak ceiling = 20*log10(50/56.5685)-1 = -2.07 dBFS, approximately **-2.1 dBFS**.
- V_rms_max = sqrt(300*4) = 34.6410 V RMS.
- RMS limit = 20*log10(34.6410/40)-3 = -4.25 dBFS, approximately **-4.3 dBFS**.
- V_allow_min = 0.9*12/0.8 = 13.5 V peak.
- Excursion limit = 20*log10(13.5/56.5685)-0 = -12.44 dBFS, approximately **-12.5 dBFS**.

This is a synthetic calculator test vector only, not recommended settings for a real speaker or amplifier.
