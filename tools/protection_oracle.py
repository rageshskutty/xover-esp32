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
