#!/usr/bin/env python3
"""Compare DSP response evaluation against scipy.signal.sosfreqz across 3 filter families."""
import math
import numpy as np
import scipy.signal as sig
from gen_golden import butter_sos, lr_sos, rbj, FS

def evaluate_sos_cascade(sos_list, freqs, fs=FS):
    """Reference double-precision evaluation matching dsp_sos_eval algorithm."""
    arr = np.array([[np.float32(s[0]), np.float32(s[1]), np.float32(s[2]), 1.0, np.float32(s[3]), np.float32(s[4])]
                    for s in sos_list], dtype=np.float64)
    w = 2.0 * np.pi * np.array(freqs, dtype=np.float64) / fs
    c1 = np.cos(w)
    s1 = np.sin(w)
    c2 = np.cos(2.0 * w)
    s2 = np.sin(2.0 * w)

    tr = np.ones_like(w, dtype=np.float64)
    ti = np.zeros_like(w, dtype=np.float64)

    for s in arr:
        b0, b1, b2, a0, a1, a2 = s
        nr = b0 + b1 * c1 + b2 * c2
        ni = -(b1 * s1 + b2 * s2)
        dr = 1.0 + a1 * c1 + a2 * c2
        di = -(a1 * s1 + a2 * s2)
        den = dr * dr + di * di
        hr = (nr * dr + ni * di) / den
        hi = (ni * dr - nr * di) / den
        ntr = tr * hr - ti * hi
        nti = tr * hi + ti * hr
        tr = ntr
        ti = nti

    return tr, ti

def main():
    print("=== SciPy sosfreqz Comparison Test (10 Hz - 20 kHz) ===")
    n_points = 1054
    # Logarithmic grid matching dsp_log_grid
    ratio = 20000.0 / 10.0
    grid = [10.0 * (ratio ** (i / (n_points - 1))) for i in range(n_points)]

    families = {
        "Butterworth": [],
        "Linkwitz-Riley": [],
        "RBJ EQ": []
    }

    # 1. Butterworth
    for o in range(1, 9):
        for m in ("lowpass", "highpass"):
            for fc in (100.0, 1000.0, 5000.0):
                families["Butterworth"].append((f"BW o={o} {m} fc={fc}", butter_sos(o, m, fc)))

    # 2. Linkwitz-Riley
    for o in (2, 4, 6, 8):
        for m in ("lowpass", "highpass"):
            for fc in (100.0, 1000.0, 5000.0):
                families["Linkwitz-Riley"].append((f"LR o={o} {m} fc={fc}", lr_sos(o, m, fc)))

    # 3. RBJ EQ
    for f0 in (200.0, 1000.0, 4000.0):
        for q in (0.7, 2.0):
            for g in (-6.0, 6.0):
                families["RBJ EQ"].append((f"peq f0={f0} q={q} g={g}", [rbj('peq', f0, q=q, g=g)]))
        for g in (-6.0, 6.0):
            for sh in ('low', 'high'):
                families["RBJ EQ"].append((f"shelf {sh} f0={f0} g={g}", [rbj('shelf', f0, g=g, shape=sh)]))
        for q in (0.7, 2.0):
            families["RBJ EQ"].append((f"allpass f0={f0} q={q}", [rbj('allpass', f0, q=q)]))
            families["RBJ EQ"].append((f"notch f0={f0} q={q}", [rbj('notch', f0, q=q)]))

    overall_max_mag_err = 0.0
    overall_max_phase_err = 0.0

    for fam_name, filters in families.items():
        fam_max_mag_err = 0.0
        fam_max_phase_err = 0.0

        for desc, secs in filters:
            arr = np.array([[np.float32(s[0]), np.float32(s[1]), np.float32(s[2]), 1.0, np.float32(s[3]), np.float32(s[4])]
                            for s in secs], dtype=np.float64)
            w, h = sig.sosfreqz(arr, grid, fs=FS)
            re, im = evaluate_sos_cascade(secs, grid, fs=FS)

            mag_scipy = np.abs(h)
            mag_eval = np.sqrt(re * re + im * im)

            for i in range(len(grid)):
                m_sci = mag_scipy[i]
                m_ev = mag_eval[i]

                # Handle near-zero response explicitly
                if m_sci < 1e-6 or m_ev < 1e-6:
                    assert m_sci < 1e-5 and m_ev < 1e-5, f"Near-zero mismatch in {desc} at {grid[i]} Hz"
                    continue

                db_sci = 20.0 * np.log10(m_sci)
                db_ev = 20.0 * np.log10(m_ev)
                mag_err = abs(db_ev - db_sci)

                p_sci = np.angle(h[i]) * 180.0 / np.pi
                p_ev = math.atan2(im[i], re[i]) * 180.0 / math.pi
                phase_err = abs((p_ev - p_sci + 180.0) % 360.0 - 180.0)

                if mag_err > fam_max_mag_err:
                    fam_max_mag_err = mag_err
                if phase_err > fam_max_phase_err:
                    fam_max_phase_err = phase_err

                assert mag_err <= 0.001, f"Mag error {mag_err} > 0.001 dB in {desc} at {grid[i]} Hz"
                assert phase_err <= 0.01, f"Phase error {phase_err} > 0.01 deg in {desc} at {grid[i]} Hz"

        if fam_max_mag_err > overall_max_mag_err:
            overall_max_mag_err = fam_max_mag_err
        if fam_max_phase_err > overall_max_phase_err:
            overall_max_phase_err = fam_max_phase_err

        print(f"  {fam_name:16s} ({len(filters):2d} filters): max mag err = {fam_max_mag_err:11.4e} dB, max phase err = {fam_max_phase_err:11.4e} deg  [PASS]")

    print(f"\nOverall across all 3 filter families (1054-point grid, 10 Hz - 20 kHz):")
    print(f"  Max magnitude error: {overall_max_mag_err:.6e} dB (limit: <= 0.001 dB)")
    print(f"  Max phase error:     {overall_max_phase_err:.6e} deg (limit: <= 0.01 deg)")
    print("ALL SCIPY COMPARISON TESTS PASSED")

if __name__ == "__main__":
    main()

