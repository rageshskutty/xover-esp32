#!/usr/bin/env python3
"""Compare schema-valid JSON filter stacks with compiled C and SciPy."""
import argparse
import ctypes as c
import json
from pathlib import Path
import numpy as np
from scipy.signal import sosfreqz

ROOT = Path(__file__).resolve().parents[1]
FS, FMIN, FMAX, POINTS = 48000.0, 10.0, 20000.0, 1054

class Sos(c.Structure):
    _fields_ = [(n, c.c_float) for n in ("b0", "b1", "b2", "a1", "a2", "da1", "da2")]

class Cascade(c.Structure):
    _fields_ = [("sos", Sos * 4), ("n", c.c_uint8)]

def load(path):
    lib = c.CDLL(str(path))
    lib.dsp_log_grid.argtypes = [c.c_double, c.c_double, c.POINTER(c.c_double), c.c_size_t]
    lib.dsp_design_xo.argtypes = [c.c_int, c.c_int, c.c_uint, c.c_double, c.c_double, c.POINTER(Cascade)]
    for name, args in (
        ("dsp_design_peq", [c.c_double] * 4 + [c.POINTER(Sos)]),
        ("dsp_design_shelf", [c.c_int, c.c_double, c.c_double, c.c_double, c.POINTER(Sos)]),
        ("dsp_design_allpass", [c.c_double] * 3 + [c.POINTER(Sos)]),
        ("dsp_design_notch", [c.c_double] * 3 + [c.POINTER(Sos)])):
        getattr(lib, name).argtypes = args
    lib.dsp_sos_eval.argtypes = [c.POINTER(Sos), c.c_size_t, c.c_double, c.c_double,
                                 c.POINTER(c.c_double), c.POINTER(c.c_double)]
    return lib

def design_filter(lib, item):
    kind = item["type"]
    f, q, gain = float(item["f_hz"]), float(item["q"]), float(item["gain_db"])
    if kind in ("lp1", "hp1", "lp2", "hp2"):
        order = 1 if kind.endswith("1") else 2
        mode = 0 if kind.startswith("lp") else 1
        if order == 2 and not np.isclose(q, 1.0 / np.sqrt(2.0), rtol=0.0, atol=1e-4):
            raise ValueError(f"{kind} Q={q:g} is not represented by current Butterworth primitive")
        if gain != 0.0:
            raise ValueError(f"{kind} does not consume gain_db")
        cas = Cascade()
        rc = lib.dsp_design_xo(0, mode, order, f, FS, c.byref(cas))
        if rc != 0:
            raise ValueError(f"dsp_design_xo returned {rc} for {kind} at {f:g} Hz")
        return list(cas.sos[:cas.n])
    out = Sos()
    if kind == "peq":
        rc = lib.dsp_design_peq(f, q, gain, FS, c.byref(out))
    elif kind in ("lshelf", "hshelf"):
        if not np.isclose(q, 1.0 / np.sqrt(2.0), rtol=0.0, atol=1e-4):
            raise ValueError(f"{kind} Q is not represented by current shelf primitive")
        rc = lib.dsp_design_shelf(0 if kind == "lshelf" else 1, f, gain, FS, c.byref(out))
    elif kind == "ap2":
        if gain != 0.0: raise ValueError("ap2 does not consume gain_db")
        rc = lib.dsp_design_allpass(f, q, FS, c.byref(out))
    elif kind == "notch":
        if gain != 0.0: raise ValueError("notch does not consume gain_db")
        rc = lib.dsp_design_notch(f, q, FS, c.byref(out))
    else:
        raise ValueError(f"no matching current dspblocks designer for {kind!r}")
    if rc != 0: raise ValueError(f"designer returned {rc} for {kind} at {f:g} Hz")
    return [out]

def compare_stack(lib, grid, filters, label):
    if not filters: return 0.0, 0.0, 0
    sections = []
    for item in filters: sections.extend(design_filter(lib, item))
    if not sections or len(sections) > 96:
        raise AssertionError(f"{label}: invalid expanded section count {len(sections)}")
    arr = np.asarray([[s.b0,s.b1,s.b2,1.0,s.a1+s.da1,s.a2+s.da2] for s in sections])
    _, ref = sosfreqz(arr, worN=grid, fs=FS)
    csos = (Sos * len(sections))(*sections)
    re, im = c.c_double(), c.c_double()
    actual = np.empty(grid.size, dtype=np.complex128)
    for i, f in enumerate(grid):
        rc = lib.dsp_sos_eval(csos, len(sections), FS, float(f), c.byref(re), c.byref(im))
        if rc != 0: raise AssertionError(f"{label}: C evaluation {rc} at {f:g} Hz")
        actual[i] = complex(re.value, im.value)
    rm, am = np.abs(ref), np.abs(actual)
    nz = (rm >= 1e-6) & (am >= 1e-6)
    if np.any((~nz) & ((rm >= 1e-5) | (am >= 1e-5))):
        raise AssertionError(f"{label}: near-zero mismatch")
    mag = phase = 0.0
    if np.any(nz):
        mag = float(np.max(np.abs(20.0*np.log10(am[nz]/rm[nz]))))
        phase = float(np.max(np.abs(np.angle(actual[nz]/ref[nz],deg=True))))
        if mag > 0.001 or phase > 0.01:
            raise AssertionError(f"{label}: {mag:.6g} dB, {phase:.6g} deg")
    return mag, phase, len(sections)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    libpath = args.library
    if libpath is None:
        candidates = list((ROOT/"host"/"build").rglob("*dspblocks_eval*"))
        libpath = next((p for p in candidates if p.is_file() and p.suffix.lower() in (".so",".dll",".dylib")),None)
    if libpath is None or not libpath.is_file(): parser.error("build host dspblocks_eval first")
    import jsonschema
    schema = json.loads((ROOT/"configs"/"schema.json").read_text(encoding="utf-8"))
    jsonschema.Draft202012Validator.check_schema(schema)
    validator = jsonschema.Draft202012Validator(schema)
    examples = sorted((ROOT/"configs").glob("example_*.json"))
    if not examples: raise SystemExit("no positive JSON examples found")
    configs = []
    for path in examples:
        cfg = json.loads(path.read_text(encoding="utf-8"))
        errors = list(validator.iter_errors(cfg))
        if errors: raise AssertionError(f"{path.name} schema error: {errors[0].message}")
        configs.append((path.name,cfg))
    lib = load(libpath)
    raw = (c.c_double*POINTS)()
    if lib.dsp_log_grid(FMIN,FMAX,raw,POINTS) != 0: raise RuntimeError("C grid failed")
    grid = np.ctypeslib.as_array(raw).copy()
    if grid[0] != FMIN or grid[-1] != FMAX or not np.all(np.diff(grid)>0):
        raise AssertionError("C grid endpoints/order are wrong")
    tested=0; worst_mag=worst_phase=0.0
    print(f"Schema-valid examples: {len(configs)}; C vs SciPy {FMIN:g}..{FMAX:g} Hz")
    for name,cfg in configs:
        stacks=[(f"{name}:pre:{p['channel']}",p["filters"]) for p in cfg.get("pre",[])]
        stacks += [(f"{name}:output:{o['id']}",o["filters"]) for o in cfg["outputs"]]
        stacks += [(f"{name}:weighting:{p['id']}",p["weighting"]["filters"])
                   for p in cfg.get("protection",{}).get("outputs",[])]
        for label,filters in stacks:
            mag,phase,n=compare_stack(lib,grid,filters,label)
            if n:
                tested+=1; worst_mag=max(worst_mag,mag); worst_phase=max(worst_phase,phase)
                print(f"  {label}: {n} sections, {mag:.3e} dB, {phase:.3e} deg [PASS]")
    if tested==0: raise AssertionError("no non-empty filter stacks in examples")
    print(f"PASS: {tested} schema-derived stacks; worst {worst_mag:.3e} dB, {worst_phase:.3e} deg")

if __name__ == "__main__": main()
