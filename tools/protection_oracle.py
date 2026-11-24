#!/usr/bin/env python3
"""Independent NumPy/SciPy reference for parametric-filter response reports.

Host-side numerical oracle for the device JSON in configs/schema.json (ADR 0014
section 4). It is not firmware, not a limit generator and not a safety proof:
it never reads driver, amplifier or supply data and it makes no pass/fail
decision against a limit. It reports, per output, the conservative static
magnitude bound of the signal path and the magnitude of the peak-detector
weighting, for the speaker designer to judge.

Filter designs mirror firmware/components/dspblocks/dspblocks.c, including the
float32 coefficient rounding and the split hi/lo denominator of lp/hp types.
Reporting conventions that ADR 0014 does not define: the -200 dB floor for a
zero magnitude, and rejecting a non-zero gain_db on filter types without gain.
"""
import json
import math
from pathlib import Path

import numpy as np
from scipy.signal import sosfreqz

FS_HZ = 48000.0
ZERO_FLOOR_DB = -200.0
# Bounds follow configs/schema.json ($defs/filter, mix, output).
F_MIN_HZ = 20.0
F_MAX_FRACTION = 0.45  # f_hz < 0.45 * fs
Q_RANGE = (0.1, 20.0)
FILTER_GAIN_DB = (-24.0, 12.0)  # boost capped at +12 dB, same as dsp_design_peq/_shelf (Q2)
OUTPUT_GAIN_DB = (-60.0, 12.0)
WEIGHT_LIMIT = 4.0
GAIN_TYPES = ("peq", "lshelf", "hshelf")
FILTER_TYPES = ("lp1", "hp1", "lp2", "hp2", "peq", "lshelf", "hshelf",
                "ap1", "ap2", "notch")


def _finite_array(value, name, *, ndim=None):
    arr = np.asarray(value, dtype=np.float64)
    if ndim is not None and arr.ndim != ndim:
        raise ValueError(f"{name} must have {ndim} dimensions")
    if not np.all(np.isfinite(arr)):
        raise ValueError(f"{name} must be finite")
    return arr


def _grid(frequencies_hz, fs_hz):
    f = _finite_array(frequencies_hz, "frequencies_hz", ndim=1)
    if f.size == 0 or np.any(f <= 0.0) or np.any(f > fs_hz / 2.0):
        raise ValueError("frequencies must be in (0, Nyquist]")
    return f


def _to_db(magnitude):
    out = np.full(magnitude.shape, ZERO_FLOOR_DB, dtype=np.float64)
    nonzero = magnitude > 0.0
    floor = 10.0 ** (ZERO_FLOOR_DB / 20.0)
    out[nonzero] = 20.0 * np.log10(np.maximum(magnitude[nonzero], floor))
    return out


def response_magnitude(sos, frequencies_hz, fs_hz):
    """Evaluate SOS rows with SciPy; each row is [b0,b1,b2,1,a1,a2]."""
    rows = _finite_array(sos, "sos", ndim=2)
    if not math.isfinite(fs_hz) or fs_hz <= 0.0:
        raise ValueError("fs_hz must be finite and positive")
    f = _grid(frequencies_hz, fs_hz)
    if rows.shape[1] != 6 or rows.shape[0] == 0:
        raise ValueError("sos must be a non-empty Nx6 array")
    _, h = sosfreqz(rows, worN=f, fs=fs_hz)
    mag = np.abs(h)
    if not np.all(np.isfinite(mag)):
        raise ValueError("SOS response is non-finite")
    return mag


def conservative_bound_db(weights, magnitudes):
    """Per-frequency bound 20*log10(sum_i |w_i| * |H_i|), in dB.

    magnitudes is shaped (inputs, frequencies). No phase-cancellation credit:
    absolute weights are summed, so an L-R difference reports twice the
    single-input bound (ADR 0014 section 4).
    """
    w = _finite_array(weights, "weights", ndim=1)
    h = _finite_array(magnitudes, "magnitudes", ndim=2)
    if w.size == 0 or h.shape[0] != w.size or h.shape[1] == 0:
        raise ValueError("weights and magnitudes dimensions disagree")
    if np.any(h < 0.0):
        raise ValueError("magnitudes must be non-negative")
    return _to_db(np.sum(np.abs(w)[:, None] * h, axis=0, dtype=np.float64))


def _row(b, a, *, split_denominator=False):
    """Build [b0,b1,b2,1,a1,a2] as the firmware stores it (float32)."""
    b32 = [float(np.float32(x)) for x in b]
    if split_denominator:
        a_eff = []
        for x in a:
            hi = float(np.float32(x))
            a_eff.append(hi + float(np.float32(x - hi)))
    else:
        a_eff = [float(np.float32(x)) for x in a]
    row = np.asarray(b32 + [1.0] + a_eff, dtype=np.float64)
    if not np.all(np.isfinite(row)):
        raise ValueError("filter coefficients must be finite")
    if np.any(np.abs(np.roots([1.0, row[4], row[5]])) >= 1.0):
        raise ValueError("filter denominator is unstable")
    return row


def _lp_hp(kind, f, q, fs):
    k = math.tan(math.pi * f / fs)
    low = kind.startswith("lp")
    if kind.endswith("1"):
        n = 1.0 + k
        b0 = (k if low else 1.0) / n
        return _row([b0, b0 if low else -1.0 / n, 0.0],
                    [(k - 1.0) / n, 0.0], split_denominator=True)
    n = 1.0 + k / q + k * k
    b0 = float(np.float32((k * k if low else 1.0) / n))
    return _row([b0, (2.0 if low else -2.0) * b0, b0],
                [2.0 * (k * k - 1.0) / n, (1.0 - k / q + k * k) / n],
                split_denominator=True)


def _peq(f, q, gain_db, fs):
    w, amp = 2.0 * math.pi * f / fs, 10.0 ** (gain_db / 40.0)
    alpha, c = math.sin(w) / (2.0 * q), math.cos(w)
    a0 = 1.0 + alpha / amp
    b1 = -2.0 * c / a0
    return _row([(1.0 + alpha * amp) / a0, b1, (1.0 - alpha * amp) / a0],
                [b1, (1.0 - alpha / amp) / a0])


def _shelf(kind, f, gain_db, fs):
    w, amp = 2.0 * math.pi * f / fs, 10.0 ** (gain_db / 40.0)
    c, s = math.cos(w), math.sin(w) * math.sqrt(2.0 * amp)
    sign = 1.0 if kind == "lshelf" else -1.0
    b0 = amp * ((amp + 1.0) - sign * (amp - 1.0) * c + s)
    b1 = sign * 2.0 * amp * ((amp - 1.0) - sign * (amp + 1.0) * c)
    b2 = amp * ((amp + 1.0) - sign * (amp - 1.0) * c - s)
    a0 = (amp + 1.0) + sign * (amp - 1.0) * c + s
    a1 = -sign * 2.0 * ((amp - 1.0) + sign * (amp + 1.0) * c)
    a2 = (amp + 1.0) + sign * (amp - 1.0) * c - s
    return _row([b0 / a0, b1 / a0, b2 / a0], [a1 / a0, a2 / a0])


def _notch_allpass(kind, f, q, fs):
    w = 2.0 * math.pi * f / fs
    if kind == "ap1":
        k = math.tan(math.pi * f / fs)
        a = (k - 1.0) / (k + 1.0)
        return _row([a, 1.0, 0.0], [a, 0.0])
    alpha = math.sin(w) / (2.0 * q)
    a0, b1 = 1.0 + alpha, -2.0 * math.cos(w) / (1.0 + alpha)
    if kind == "notch":
        return _row([1.0 / a0, b1, 1.0 / a0], [b1, (1.0 - alpha) / a0])
    b0 = (1.0 - alpha) / a0
    return _row([b0, b1, 1.0], [b1, b0])


def design_filter(filt, fs_hz):
    """Return one SOS row for a schema filter entry; raise on anything else."""
    kind = filt.get("type") if isinstance(filt, dict) else None
    if kind not in FILTER_TYPES:
        raise ValueError(f"unsupported filter type: {kind!r}")
    try:
        f, q, gain = (float(filt[key]) for key in ("f_hz", "q", "gain_db"))
    except (KeyError, TypeError) as exc:
        raise ValueError(f"filter {kind} needs numeric f_hz, q and gain_db") from exc
    f_max = F_MAX_FRACTION * fs_hz
    if not (math.isfinite(f) and F_MIN_HZ <= f < f_max):
        raise ValueError(f"f_hz must be finite and in [{F_MIN_HZ:g}, {f_max:g})")
    if not (math.isfinite(q) and Q_RANGE[0] <= q <= Q_RANGE[1]):
        raise ValueError(f"q must be finite and in [{Q_RANGE[0]:g}, {Q_RANGE[1]:g}]")
    if not (math.isfinite(gain) and FILTER_GAIN_DB[0] <= gain <= FILTER_GAIN_DB[1]):
        raise ValueError(f"gain_db must be finite and in {list(FILTER_GAIN_DB)}")
    if kind not in GAIN_TYPES and gain != 0.0:
        raise ValueError(f"gain_db is not defined for filter type {kind}")
    if kind in ("lp1", "hp1", "lp2", "hp2"):
        return _lp_hp(kind, f, q, fs_hz)
    if kind == "peq":
        return _peq(f, q, gain, fs_hz)
    if kind in ("lshelf", "hshelf"):
        return _shelf(kind, f, gain, fs_hz)
    return _notch_allpass(kind, f, q, fs_hz)


def filter_response(filters, frequencies_hz, fs_hz=FS_HZ):
    """Complex response of an ordered schema filters[] list; [] is identity."""
    f = _grid(frequencies_hz, fs_hz)
    rows = [design_filter(item, fs_hz) for item in filters]
    if not rows:
        return np.ones(f.size, dtype=np.complex128)
    _, h = sosfreqz(np.asarray(rows), worN=f, fs=fs_hz)
    if not np.all(np.isfinite(h.real)) or not np.all(np.isfinite(h.imag)):
        raise ValueError("filter response is non-finite")
    return h


def _mix_weight(value):
    weight = float(value)
    if not (math.isfinite(weight) and weight != 0.0 and abs(weight) <= WEIGHT_LIMIT):
        raise ValueError("weight must be finite, non-zero and within [-4, 4]")
    return weight


def evaluate_config(config, frequencies_hz):
    """Report static magnitudes per output id on the supplied frequency grid.

    Returns {output_id: {"bound_db": array, "weighting_db": array or None}}.
    bound_db is the conservative bound of input mix, pre filters, output
    filters and output gain (polarity and delay do not change magnitude).
    weighting_db is the peak-detector weighting magnitude, or None when the
    config carries no protection record for that output. It is a report for
    the speaker designer, not a firmware decision.
    """
    if config.get("v") != 1 or float(config.get("fs", 0.0)) != FS_HZ:
        raise ValueError("unsupported config version or sample rate")
    f = _grid(frequencies_hz, FS_HZ)
    pre_items = config.get("pre", [])
    channels = [item["channel"] for item in pre_items]
    if len(channels) != len(set(channels)):
        raise ValueError("duplicate pre channel")
    pre = {item["channel"]: np.abs(filter_response(item["filters"], f)) for item in pre_items}
    ids = [out["id"] for out in config["outputs"]]
    if len(ids) != len(set(ids)):
        raise ValueError("duplicate output id")
    records = {rec["id"]: rec for rec in config.get("protection", {}).get("outputs", [])}
    flat = np.ones(f.size)
    report = {}
    for out in config["outputs"]:
        gain_db = float(out["gain_db"])
        if not (math.isfinite(gain_db) and OUTPUT_GAIN_DB[0] <= gain_db <= OUTPUT_GAIN_DB[1]):
            raise ValueError(f"gain_db must be finite and in {list(OUTPUT_GAIN_DB)}")
        chain = np.abs(filter_response(out["filters"], f)) * 10.0 ** (gain_db / 20.0)
        weights = [_mix_weight(mix["weight"]) for mix in out["in"]]
        mags = [pre.get(mix["channel"], flat) * chain for mix in out["in"]]
        record = records.get(out["id"])
        weighting = None
        if record is not None:
            weighting = _to_db(np.abs(filter_response(record["weighting"]["filters"], f)))
        report[out["id"]] = {"bound_db": conservative_bound_db(weights, mags),
                             "weighting_db": weighting}
    return report


def main():
    path = Path(__file__).resolve().parent / "protection_oracle_vectors.json"
    vectors = json.loads(path.read_text(encoding="utf-8"))
    failures = []
    for v in vectors["filters"]:
        got = _to_db(np.abs(filter_response(v["filters"], v["frequencies_hz"])))
        if "max_db" in v:
            ok = bool(np.all(got <= v["max_db"]))
        else:
            ok = bool(np.all(np.abs(got - np.asarray(v["expected_db"])) <= v["tol_db"]))
        if not ok:
            failures.append(f'{v["name"]}: got {got.tolist()}')
    for v in vectors["bounds"]:
        got = conservative_bound_db(v["weights"], v["magnitudes"])
        if not np.allclose(got, v["expected_bound_db"], rtol=0.0, atol=1e-9):
            failures.append(f'{v["name"]}: got {got.tolist()}')
    if failures:
        raise SystemExit("\n".join(failures))
    print(f'PASS: {len(vectors["filters"])} filter and {len(vectors["bounds"])} bound vectors')


if __name__ == "__main__":
    main()
