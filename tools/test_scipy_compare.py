#!/usr/bin/env python3
"""Compare compiled dsp_sos_eval with scipy.signal.sosfreqz on the T04 grid."""
import argparse
import ctypes as c
from pathlib import Path
import numpy as np
from scipy.signal import sosfreqz

FS, FMIN, FMAX, POINTS = 48000.0, 10.0, 20000.0, 1054

class Sos(c.Structure):
    _fields_ = [(n, c.c_float) for n in ("b0", "b1", "b2", "a1", "a2", "da1", "da2")]

class Cascade(c.Structure):
    _fields_ = [("sos", Sos * 4), ("n", c.c_uint8)]


def load(path):
    lib = c.CDLL(str(path))
    lib.dsp_log_grid.argtypes = [c.c_double, c.c_double, c.POINTER(c.c_double), c.c_size_t]
    lib.dsp_design_xo.argtypes = [c.c_int, c.c_int, c.c_uint, c.c_double, c.c_double, c.POINTER(Cascade)]
    for name, args in (("dsp_design_peq", [c.c_double] * 4 + [c.POINTER(Sos)]),
                       ("dsp_design_shelf", [c.c_int, c.c_double, c.c_double, c.c_double, c.POINTER(Sos)]),
                       ("dsp_design_allpass", [c.c_double] * 3 + [c.POINTER(Sos)]),
                       ("dsp_design_notch", [c.c_double] * 3 + [c.POINTER(Sos)])):
        getattr(lib, name).argtypes = args
    lib.dsp_sos_eval.argtypes = [c.POINTER(Sos), c.c_size_t, c.c_double, c.c_double,
                                 c.POINTER(c.c_double), c.POINTER(c.c_double)]
    return lib


def designs(lib):
    result = {"Butterworth": [], "Linkwitz-Riley": [], "RBJ EQ": []}
    for fam, orders in ((0, range(1, 9)), (1, (2, 4, 6, 8))):
        for order in orders:
            for mode in (0, 1):
                for fc in (100.0, 1000.0, 5000.0):
                    cas = Cascade()
                    assert lib.dsp_design_xo(fam, mode, order, fc, FS, c.byref(cas)) == 0
                    result[("Butterworth", "Linkwitz-Riley")[fam]].append((list(cas.sos[:cas.n]), f"{order=} {mode=} {fc=}"))
    eq = result["RBJ EQ"]
    for f in (200.0, 1000.0, 4000.0):
        for q in (0.7, 2.0):
            for gain in (-6.0, 6.0):
                s = Sos(); assert lib.dsp_design_peq(f, q, gain, FS, c.byref(s)) == 0
                eq.append(([s], f"peq {f=} {q=} {gain=}"))
            for shape in (0, 1):
                s = Sos(); assert lib.dsp_design_shelf(shape, f, 6.0, FS, c.byref(s)) == 0
                eq.append(([s], f"shelf {f=} {shape=}"))
            for fn in (lib.dsp_design_allpass, lib.dsp_design_notch):
                s = Sos(); assert fn(f, q, FS, c.byref(s)) == 0
                eq.append(([s], f"{fn.__name__} {f=} {q=}"))
    return result


def compare(lib, grid, families):
    re, im = c.c_double(), c.c_double()
    for family, cases in families.items():
        worst_mag = worst_phase = 0.0
        for sections, label in cases:
            csos = (Sos * len(sections))(*sections)
            arr = np.array([[s.b0, s.b1, s.b2, 1.0, s.a1 + s.da1, s.a2 + s.da2] for s in sections])
            _, ref = sosfreqz(arr, worN=grid, fs=FS)
            actual = np.empty(POINTS, dtype=np.complex128)
            for i, f in enumerate(grid):
                rc = lib.dsp_sos_eval(csos, len(sections), FS, f, c.byref(re), c.byref(im))
                if rc: raise AssertionError(f"C evaluation {rc}: {family} {label} at {f:g} Hz")
                actual[i] = complex(re.value, im.value)
            rm, am = np.abs(ref), np.abs(actual)
            nz = (rm >= 1e-6) & (am >= 1e-6)
            if np.any((~nz) & ((rm >= 1e-5) | (am >= 1e-5))):
                raise AssertionError(f"near-zero mismatch: {family} {label}")
            if np.any(nz):
                mag = float(np.max(np.abs(20 * np.log10(am[nz] / rm[nz]))))
                phase = float(np.max(np.abs(np.angle(actual[nz] / ref[nz], deg=True))))
                worst_mag, worst_phase = max(mag, worst_mag), max(phase, worst_phase)
                if mag > 0.001 or phase > 0.01:
                    raise AssertionError(f"{family} {label}: {mag:.6g} dB, {phase:.6g} deg")
        print(f"{family:16} {len(cases):3} filters: {worst_mag:.3e} dB, {worst_phase:.3e} deg [PASS]")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1] / "host" / "build"
    files = [args.library] if args.library else list(root.rglob("*dspblocks_eval*"))
    libpath = next((p for p in files if p and p.is_file() and p.suffix.lower() in (".so", ".dll", ".dylib")), None)
    if libpath is None: parser.error("build the host dspblocks_eval shared library first")
    lib = load(libpath)
    raw = (c.c_double * POINTS)()
    if lib.dsp_log_grid(FMIN, FMAX, raw, POINTS): raise RuntimeError("C dsp_log_grid failed")
    grid = np.ctypeslib.as_array(raw).copy()
    assert grid[0] == FMIN and grid[-1] == FMAX and np.all(np.diff(grid) > 0)
    print(f"Compiled C vs SciPy, {POINTS}-point grid: {FMIN:g}..{FMAX:g} Hz")
    compare(lib, grid, designs(lib))
    print("PASS: magnitude <= 0.001 dB; phase <= 0.01 deg")

if __name__ == "__main__":
    main()
