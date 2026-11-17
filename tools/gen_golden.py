#!/usr/bin/env python3
"""Generate golden filter test vectors using scipy.signal."""
import json, os, numpy as np, scipy.signal as sig

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
GOLDEN_DIR = os.path.join(ROOT, "host", "tests", "golden")
FS = 48000.0

def biquad(mode, fc, q, fs=FS):
    K = np.tan(np.pi * fc / fs)
    if q <= 0:
        n = 1.0 + K
        return [float((K if mode == 'lowpass' else 1.0)/n), float((K if mode == 'lowpass' else -1.0)/n), 0.0, float((K - 1.0)/n), 0.0]
    n = 1.0 + K / q + K * K
    b0 = float((K * K if mode == 'lowpass' else 1.0) / n)
    return [b0, float((2.0 if mode == 'lowpass' else -2.0) * b0), b0, float(2.0 * (K * K - 1.0) / n), float((1.0 - K / q + K * K) / n)]

def butter_sos(order, mode, fc):
    if order % 2 == 0:
        qs = [1.0 / (2.0 * np.cos((2 * k - 1) * np.pi / (2 * order))) for k in range(1, order // 2 + 1)]
    else:
        qs = [1.0 / (2.0 * np.cos(k * np.pi / order)) for k in range(1, (order - 1) // 2 + 1)] + [-1.0]
    return [biquad(mode, fc, q) for q in qs]

def lr_sos(order, mode, fc):
    lr_qs = {2: [0.5], 4: [1/np.sqrt(2)]*2, 6: [1.0, 1.0, 0.5], 8: [1/(2*np.cos(np.pi/8))]*2 + [1/(2*np.cos(3*np.pi/8))]*2}
    return [biquad(mode, fc, q) for q in lr_qs[order]]

def rbj(kind, f0, q=0.0, g=0.0, shape='low'):
    w0 = 2 * np.pi * f0 / FS; c = np.cos(w0); s = np.sin(w0); a = s / (2 * q) if q > 0 else 0.0
    if kind == 'peq':
        A = 10**(g/40); a0 = 1 + a/A
        return [float((1+a*A)/a0), float(-2*c/a0), float((1-a*A)/a0), float(-2*c/a0), float((1-a/A)/a0)]
    if kind == 'allpass':
        return [float((1-a)/(1+a)), float(-2*c/(1+a)), 1.0, float(-2*c/(1+a)), float((1-a)/(1+a))]
    if kind == 'notch':
        return [float(1/(1+a)), float(-2*c/(1+a)), float(1/(1+a)), float(-2*c/(1+a)), float((1-a)/(1+a))]
    A = 10**(g/40); st = s * np.sqrt(2 * A)
    if shape == 'low':
        b0, b1, b2 = A*((A+1)-(A-1)*c+st), 2*A*((A-1)-(A+1)*c), A*((A+1)-(A-1)*c-st)
        a0, a1, a2 = (A+1)+(A-1)*c+st, -2*((A-1)+(A+1)*c), (A+1)+(A-1)*c-st
    else:
        b0, b1, b2 = A*((A+1)+(A-1)*c+st), -2*A*((A-1)+(A+1)*c), A*((A+1)+(A-1)*c-st)
        a0, a1, a2 = (A+1)-(A-1)*c+st, 2*((A-1)-(A+1)*c), (A+1)-(A-1)*c-st
    return [float(b0/a0), float(b1/a0), float(b2/a0), float(a1/a0), float(a2/a0)]

def main():
    os.makedirs(GOLDEN_DIR, exist_ok=True)
    grid = [10.0, 100.0, 1000.0, 10000.0, 20000.0]
    def eval_s(secs):
        arr = np.array([[s[0], s[1], s[2], 1.0, s[3], s[4]] for s in secs])
        w, h = sig.sosfreqz(arr, grid, fs=FS)
        return (20*np.log10(np.abs(h))).tolist(), (np.unwrap(np.angle(h))*180/np.pi).tolist()

    bw = [{"family": "Butterworth", "order": o, "mode": m, "fc_hz": fc, "sections": butter_sos(o, m, fc),
           "mag_db": eval_s(butter_sos(o, m, fc))[0]} for o in range(1, 9) for m in ("lowpass", "highpass") for fc in (100.0, 1000.0, 5000.0)]
    lr = [{"family": "Linkwitz-Riley", "order": o, "mode": m, "fc_hz": fc, "sections": lr_sos(o, m, fc),
           "mag_db": eval_s(lr_sos(o, m, fc))[0]} for o in (2, 4, 6, 8) for m in ("lowpass", "highpass") for fc in (100.0, 1000.0, 5000.0)]
    eq = []
    for f0 in (200.0, 1000.0, 4000.0):
        for q in (0.7, 2.0):
            for g in (-6.0, 6.0): eq.append({"type": "peq", "f0_hz": f0, "q": q, "gain_db": g, "sos": rbj('peq', f0, q=q, g=g)})
        for g in (-6.0, 6.0):
            for sh in ('low', 'high'): eq.append({"type": "shelf", "shape": sh, "f0_hz": f0, "gain_db": g, "sos": rbj('shelf', f0, g=g, shape=sh)})
        for q in (0.7, 2.0):
            eq.append({"type": "allpass", "f0_hz": f0, "q": q, "sos": rbj('allpass', f0, q=q)})
            eq.append({"type": "notch", "f0_hz": f0, "q": q, "sos": rbj('notch', f0, q=q)})

    with open(os.path.join(GOLDEN_DIR, "butterworth.json"), "w") as f: json.dump(bw, f, indent=2)
    with open(os.path.join(GOLDEN_DIR, "linkwitz_riley.json"), "w") as f: json.dump(lr, f, indent=2)
    with open(os.path.join(GOLDEN_DIR, "rbj_eq.json"), "w") as f: json.dump(eq, f, indent=2)

    with open(os.path.join(GOLDEN_DIR, "golden_vectors.h"), "w") as f:
        f.write("#pragma once\n#include \"dspblocks.h\"\n")
        f.write("typedef struct { dsp_family_t family; dsp_mode_t mode; unsigned order; double fc_hz, fs_hz; uint8_t n_sections; dsp_sos_t expected_sos[4]; } golden_xo_entry_t;\n")
        f.write("static const golden_xo_entry_t GOLDEN_XO_ENTRIES[] = {\n")
        for e in bw + lr:
            fam = "DSP_FAMILY_BUTTERWORTH" if e["family"] == "Butterworth" else "DSP_FAMILY_LINKWITZ_RILEY"
            m = "DSP_MODE_LOWPASS" if e["mode"] == "lowpass" else "DSP_MODE_HIGHPASS"
            secs = e["sections"] + [[0.0]*5]*(4 - len(e["sections"]))
            sos_str = ", ".join(f"{{ {s[0]}f, {s[1]}f, {s[2]}f, {s[3]}f, {s[4]}f }}" for s in secs)
            f.write(f"    {{ {fam}, {m}, {e['order']}U, {e['fc_hz']}, 48000.0, {len(e['sections'])}, {{ {sos_str} }} }},\n")
        f.write(f"}};\n#define GOLDEN_XO_COUNT {len(bw) + len(lr)}U\n\n")
        f.write("typedef struct { const char *type; int shape; double f0_hz, q, gain_db, fs_hz; dsp_sos_t expected_sos; } golden_eq_entry_t;\n")
        f.write("static const golden_eq_entry_t GOLDEN_EQ_ENTRIES[] = {\n")
        for e in eq:
            sh = "DSP_SHELF_LOW" if e.get("shape") == "low" else "DSP_SHELF_HIGH"
            s = e["sos"]
            f.write(f"    {{ \"{e['type']}\", {sh}, {e['f0_hz']}, {e.get('q', 0.0)}, {e.get('gain_db', 0.0)}, 48000.0, {{ {s[0]}f, {s[1]}f, {s[2]}f, {s[3]}f, {s[4]}f }} }},\n")
        f.write(f"}};\n#define GOLDEN_EQ_COUNT {len(eq)}U\n")
    print(f"Generated {len(bw)} BW, {len(lr)} LR, {len(eq)} EQ golden vectors.")

if __name__ == "__main__":
    main()
