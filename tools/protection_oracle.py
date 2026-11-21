#!/usr/bin/env python3
"""Independent NumPy/SciPy reference for the chain-v1 protection comparison.

This is a host-side numerical oracle, not a driver-limit generator and not
production firmware. The proposed -200 dB zero-transfer floor and
log-frequency/linear-dB interpolation require ADR/CODEOWNERS approval before
the C comparator contract is frozen.
"""
import json
import math
from pathlib import Path

import numpy as np
from scipy.signal import sosfreqz

ZERO_FLOOR_DB = -200.0


def _finite_array(value, name, *, ndim=None):
    arr = np.asarray(value, dtype=np.float64)
    if ndim is not None and arr.ndim != ndim:
        raise ValueError(f"{name} must have {ndim} dimensions")
    if not np.all(np.isfinite(arr)):
        raise ValueError(f"{name} must be finite")
    return arr


def response_magnitude(sos, frequencies_hz, fs_hz):
    """Evaluate SOS rows with SciPy; each row is [b0,b1,b2,1,a1,a2]."""
    f = _finite_array(frequencies_hz, "frequencies_hz", ndim=1)
    rows = _finite_array(sos, "sos", ndim=2)
    if not math.isfinite(fs_hz) or fs_hz <= 0.0:
        raise ValueError("fs_hz must be finite and positive")
    if f.size == 0 or np.any(f <= 0.0) or np.any(f > fs_hz / 2.0):
        raise ValueError("frequencies must be in (0, Nyquist]")
    if rows.shape[1] != 6 or rows.shape[0] == 0:
        raise ValueError("sos must be a non-empty Nx6 array")
    _, h = sosfreqz(rows, worN=f, fs=fs_hz)
    mag = np.abs(h)
    if not np.all(np.isfinite(mag)):
        raise ValueError("SOS response is non-finite")
    return mag


def interpolate_limit_curve(eval_hz, knot_hz, knot_db):
    """Proposed interpolation: linear dB versus log10(f); never extrapolate."""
    f = _finite_array(eval_hz, "eval_hz", ndim=1)
    kf = _finite_array(knot_hz, "knot_hz", ndim=1)
    kd = _finite_array(knot_db, "knot_db", ndim=1)
    if f.size == 0 or kf.size < 2 or kf.size != kd.size:
        raise ValueError("invalid evaluation or curve dimensions")
    if np.any(f <= 0.0) or np.any(kf <= 0.0):
        raise ValueError("frequencies must be positive")
    if np.any(np.diff(f) <= 0.0) or np.any(np.diff(kf) <= 0.0):
        raise ValueError("frequencies and curve knots must be strictly increasing")
    if f[0] < kf[0] or f[-1] > kf[-1]:
        raise ValueError("limit curve does not cover the evaluation grid")
    return np.interp(np.log10(f), np.log10(kf), kd)


def compare_output(weights, magnitudes, limit_db, frequencies_hz, output_id):
    """Return first violation; exact equality passes; order is frequency order."""
    w = _finite_array(weights, "weights", ndim=1)
    h = _finite_array(magnitudes, "magnitudes", ndim=2)
    lim = _finite_array(limit_db, "limit_db", ndim=1)
    f = _finite_array(frequencies_hz, "frequencies_hz", ndim=1)
    if not isinstance(output_id, str) or not output_id or len(output_id.encode()) > 31:
        raise ValueError("output_id must contain 1..31 UTF-8 bytes")
    if w.size == 0 or h.shape != (w.size, f.size) or f.size == 0 or lim.size != f.size:
        raise ValueError("weights, response, limit, and frequency dimensions disagree")
    if np.any(h < 0.0) or np.any(np.diff(f) <= 0.0) or np.any(f <= 0.0):
        raise ValueError("magnitudes must be non-negative and frequencies strictly increasing")
    bound = np.sum(np.abs(w)[:, None] * h, axis=0, dtype=np.float64)
    floor_linear = 10.0 ** (ZERO_FLOOR_DB / 20.0)
    bound_db = np.full(bound.shape, ZERO_FLOOR_DB, dtype=np.float64)
    nonzero = bound > 0.0
    bound_db[nonzero] = 20.0 * np.log10(np.maximum(bound[nonzero], floor_linear))
    bad = np.flatnonzero(bound_db > lim)
    if bad.size:
        i = int(bad[0])
        return {"pass": False, "output_id": output_id, "index": i,
                "frequency_hz": float(f[i]), "bound_db": float(bound_db[i]),
                "limit_db": float(lim[i])}
    return {"pass": True, "output_id": output_id}


def compare_config(outputs, frequencies_hz):
    """Compare outputs in supplied order and return the first valid violation."""
    for output in outputs:
        result = compare_output(output["weights"], output["magnitudes"],
                                output["limit_db"], frequencies_hz, output["id"])
        if not result["pass"]:
            return result
    return {"pass": True}


def _sos_row(b, a, *, split_denominator=False):
    if len(b) != 3 or len(a) != 2:
        raise ValueError("SOS requires three numerator and two denominator coefficients")
    if split_denominator:
        row = np.asarray([np.float32(b[0]), np.float32(b[1]), np.float32(b[2]),
                          1.0, a[0], a[1]], dtype=np.float64)
    else:
        row = np.asarray([b[0], b[1], b[2], 1.0, a[0], a[1]],
                         dtype=np.float32).astype(np.float64)
    if not np.all(np.isfinite(row)):
        raise ValueError("SOS coefficients must be finite")
    poles = np.roots([1.0, row[4], row[5]])
    if np.any(np.abs(poles) >= 1.0):
        raise ValueError("SOS denominator is unstable")
    return row


def _design_biquad(block, fs_hz):
    kind, f, q = block["k"], float(block["f_hz"]), float(block["q"])
    if not (math.isfinite(f) and 10.0 <= f <= 0.45 * fs_hz and
            math.isfinite(q) and 0.0 < q <= 20.0):
        raise ValueError("invalid biquad frequency or Q")
    w = 2.0 * math.pi * f / fs_hz
    if kind in ("lp", "hp"):
        k = math.tan(math.pi * f / fs_hz)
        n = 1.0 + k / q + k * k
        b0 = (k * k if kind == "lp" else 1.0) / n
        b1 = (2.0 if kind == "lp" else -2.0) * b0
        b2 = b0
        a1 = 2.0 * (k * k - 1.0) / n
        a2 = (1.0 - k / q + k * k) / n
        # Mirror the C implementation's split float32 denominator.
        a1_hi = float(np.float32(a1))
        a2_hi = float(np.float32(a2))
        a1 = a1_hi + float(np.float32(a1 - a1_hi))
        a2 = a2_hi + float(np.float32(a2 - a2_hi))
        return _sos_row([b0, b1, b2], [a1, a2], split_denominator=True)
    if kind == "peq":
        gain = float(block["db"])
        if not math.isfinite(gain) or not -24.0 <= gain <= 12.0 or q < 0.1:
            raise ValueError("invalid PEQ gain or Q")
        alpha = math.sin(w) / (2.0 * q)
        A = 10.0 ** (gain / 40.0)
        c = math.cos(w)
        a0 = 1.0 + alpha / A
        b = [(1.0 + alpha * A) / a0, -2.0 * c / a0,
             (1.0 - alpha * A) / a0]
        a = [-2.0 * c / a0, (1.0 - alpha / A) / a0]
        return _sos_row(b, a)
    if kind == "notch":
        alpha = math.sin(w) / (2.0 * q)
        a0 = 1.0 + alpha
        b = [1.0 / a0, -2.0 * math.cos(w) / a0, 1.0 / a0]
        a = [-2.0 * math.cos(w) / a0, (1.0 - alpha) / a0]
        return _sos_row(b, a)
    if kind == "allpass":
        alpha = math.sin(w) / (2.0 * q)
        a0 = 1.0 + alpha
        b0 = (1.0 - alpha) / a0
        b1 = -2.0 * math.cos(w) / a0
        b = [b0, b1, 1.0]
        a = [b1, b0]
        return _sos_row(b, a)
    raise ValueError(f"unsupported biquad kind: {kind}")


def chain_response(chain, frequencies_hz, fs_hz):
    """Evaluate an ordered chain independently using NumPy/SciPy."""
    f = _finite_array(frequencies_hz, "frequencies_hz", ndim=1)
    if f.size == 0 or np.any(f <= 0.0) or np.any(f > fs_hz / 2.0):
        raise ValueError("invalid chain evaluation frequencies")
    total = np.ones(f.size, dtype=np.complex128)
    for block in chain:
        kind = block.get("t")
        if kind == "biquad":
            row = _design_biquad(block, fs_hz)
            _, h = sosfreqz([row], worN=f, fs=fs_hz)
            total *= h
        elif kind == "sos":
            if block.get("fs") != fs_hz:
                raise ValueError("SOS sample-rate mismatch")
            row = _sos_row(block["b"], block["a"])
            _, h = sosfreqz([row], worN=f, fs=fs_hz)
            total *= h
        elif kind == "gain":
            db = float(block["db"])
            if not math.isfinite(db) or not -60.0 <= db <= 12.0:
                raise ValueError("invalid gain")
            total *= 10.0 ** (db / 20.0)
        elif kind == "delay":
            us = float(block["us"])
            if not math.isfinite(us) or not 0.0 <= us <= 20000.0:
                raise ValueError("invalid delay")
            total *= np.exp(-2j * np.pi * f * us * 1e-6)
        elif kind == "polarity":
            total *= -1.0 if block["invert"] else 1.0
        else:
            raise ValueError(f"unsupported chain block: {kind}")
        if not np.all(np.isfinite(total.real)) or not np.all(np.isfinite(total.imag)):
            raise ValueError("chain response is non-finite")
    return total


def evaluate_config(config, protected_curves, frequencies_hz):
    """Evaluate per-input transfers and conservative per-output bounds.

    protected_curves maps output IDs to {frequencies_hz: [...], limit_db: [...]}.
    It must contain already provisioned values; this function never derives
    driver, amplifier, supply, or safety limits.
    """
    fs_hz = float(config["fs"])
    if config.get("v") != 1 or fs_hz != 48000.0:
        raise ValueError("unsupported chain config version or sample rate")
    f = _finite_array(frequencies_hz, "frequencies_hz", ndim=1)
    if f.size == 0 or np.any(f <= 0.0) or np.any(np.diff(f) <= 0.0):
        raise ValueError("evaluation grid must be positive and strictly increasing")
    inputs = config["inputs"]
    if len(inputs) != len(set(inputs)):
        raise ValueError("duplicate input channel")
    pre = {item["channel"]: item["chain"] for item in config.get("pre", [])}
    if len(pre) != len(config.get("pre", [])) or not set(pre).issubset(inputs):
        raise ValueError("duplicate or undeclared pre-chain channel")
    outputs = config["outputs"]
    ids = [out["id"] for out in outputs]
    if len(ids) != len(set(ids)) or set(ids) != set(protected_curves):
        raise ValueError("configured outputs and protected curve IDs must match exactly")
    results = []
    for out in outputs:
        channel_responses = []
        weights = []
        seen = set()
        for mix in out["in"]:
            ch, weight = mix["channel"], float(mix["weight"])
            if ch not in inputs or ch in seen or not math.isfinite(weight) or weight == 0.0:
                raise ValueError("invalid or duplicate input mix entry")
            seen.add(ch)
            pre_h = chain_response(pre.get(ch, []), f, fs_hz)
            out_h = chain_response(out["chain"], f, fs_hz)
            channel_responses.append(np.abs(pre_h * out_h))
            weights.append(weight)
        curve = protected_curves[out["id"]]
        limit = interpolate_limit_curve(f, curve["frequencies_hz"], curve["limit_db"])
        result = compare_output(weights, np.asarray(channel_responses), limit, f, out["id"])
        results.append(result)
        if not result["pass"]:
            return result
    return {"pass": True, "outputs": results}

def main():
    root = Path(__file__).resolve().parents[1]
    path = root / "tools" / "protection_oracle_vectors.json"
    vectors = json.loads(path.read_text(encoding="utf-8"))
    failures = []
    for vector in vectors:
        got = compare_output(vector["weights"], vector["magnitudes"],
                             vector["limit_db"], vector["frequencies_hz"],
                             vector["output_id"])
        if got["pass"] != vector["expected_pass"]:
            failures.append(f'{vector["name"]}: expected {vector["expected_pass"]}, got {got}')
        if vector.get("expected_bound_db") is not None:
            if not got["pass"] or not math.isclose(got["bound_db"],
                    vector["expected_bound_db"], rel_tol=0.0, abs_tol=1e-9):
                failures.append(f'{vector["name"]}: wrong bound {got}')
    if failures:
        raise SystemExit("\n".join(failures))
    print(f"PASS: {len(vectors)} protection reference vectors")


if __name__ == "__main__":
    main()
