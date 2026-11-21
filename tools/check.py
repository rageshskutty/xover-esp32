#!/usr/bin/env python3
"""Repo guardrail checks for xover-esp32.   Run: python tools/check.py

Exit code 0 = all checks pass.  Checks:
  1. configs/*.json     must pass the JSON schema AND the semantic rules
  2. configs/invalid/*  must each FAIL with the code named in "_expect"
  3. *_rt.c / *_rt.h    must not use forbidden real-time constructs (AGENTS.md R1)
  4. no secrets committed (AGENTS.md X2)
  5. self-test of the RT scanner (so a broken scanner cannot pass silently)
"""
import glob
import json
import math
import os
import re
import struct
import sys
from collections import defaultdict

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FS = 48000
MAX_NODES = 64
MAX_SECTIONS = 96
N_SLOTS = 8
MAX_DELAY_US = 20000
FAMILIES = ("Butterworth", "Linkwitz-Riley", "Bessel")
NUMERIC_FIELDS = ("frequency_hz", "db", "q", "delay_us")
VAR_RE = re.compile(r"^@([A-Za-z_][A-Za-z0-9_]*)$")


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def f32(x):
    """Round to float32; None if not finite or out of float32 range."""
    if isinstance(x, bool) or not isinstance(x, (int, float)):
        return None
    try:
        r = struct.unpack("f", struct.pack("f", x))[0]
    except (OverflowError, struct.error):
        return None
    return r if math.isfinite(r) else None


def biquad_stable(b, a):
    """Mirror of dsp_check_stable on float32 values (float32 arithmetic)."""
    cs = [f32(v) for v in list(b) + list(a)]
    if any(v is None for v in cs):
        return False
    a1, a2 = cs[3], cs[4]
    return (abs(a2) < 1.0 and f32(f32(1.0 + a1) + a2) > 0.0
            and f32(f32(1.0 - a1) + a2) > 0.0)


def sections(node):
    t = node.get("type")
    if t == "crossover":
        n = node.get("order", 0)
        if not isinstance(n, int) or n < 1:
            return 0
        return n // 2 if node.get("family") == "Linkwitz-Riley" else (n + 1) // 2
    if t in ("peq", "shelf", "allpass", "notch", "biquad"):
        return 1
    return 0


def resolve(value, variables):
    if isinstance(value, str):
        m = VAR_RE.match(value)
        if not m:
            return None, None
        name = m.group(1)
        if name not in variables:
            return None, name
        return variables[name], None
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        return value, None
    return None, None


def semantic_errors(cfg, test_protected=None):
    """Chain-v1 cross-field checks. Hardware limits are supplied only by a
    separately provisioned protected record; tests may pass synthetic metadata."""
    errs = []
    inputs = cfg.get("inputs", [])
    pre = cfg.get("pre", [])
    outputs = cfg.get("outputs", [])
    if len(inputs) != len(set(inputs)):
        errs.append("E_SCHEMA: duplicate declared input")
    declared = set(inputs)

    pre_channels = []
    for item in pre:
        ch = item.get("channel")
        pre_channels.append(ch)
        if ch not in declared:
            errs.append("E_SCHEMA: pre channel %r is not declared in inputs" % ch)
    if len(pre_channels) != len(set(pre_channels)):
        errs.append("E_SCHEMA: pre channel must be unique")

    ids = [o.get("id") for o in outputs]
    slots = [o.get("slot") for o in outputs]
    if len(ids) != len(set(ids)):
        errs.append("E_SCHEMA: output id must be unique")
    if len(slots) != len(set(slots)):
        errs.append("E_SCHEMA: output slot must be unique")

    for out in outputs:
        seen = set()
        for item in out.get("in", []):
            ch = item.get("channel")
            if ch not in declared:
                errs.append("E_SCHEMA: output %s references undeclared input %r" %
                            (out.get("id"), ch))
            if ch in seen:
                errs.append("E_SCHEMA: output %s repeats input channel %r" %
                            (out.get("id"), ch))
            seen.add(ch)
            w = item.get("weight")
            if (isinstance(w, bool) or not isinstance(w, (int, float))
                    or not math.isfinite(w) or w == 0 or abs(w) > 4):
                errs.append("E_SCHEMA: output %s has invalid mix weight %r" %
                            (out.get("id"), w))
        if not out.get("chain"):
            errs.append("E_PROTECT: output %s has empty chain; attenuation below protected min_freq_hz is not established" %
                        out.get("id"))

    if test_protected:
        protected_ids = test_protected.get("out_ids")
        if protected_ids is not None and set(ids) != set(protected_ids):
            errs.append("E_PROTECT: designer output ids do not match protected out_id set")
        slot_count = test_protected.get("tdm_slot_count")
        if slot_count is not None:
            for slot in slots:
                if not isinstance(slot, int) or slot < 0 or slot >= slot_count:
                    errs.append("E_SCHEMA: output slot %r outside protected TDM slot count %d" %
                                (slot, slot_count))
        bounds = test_protected.get("lim_bounds", {})
        for out in outputs:
            limits = bounds.get(out.get("id"), {})
            protect = out.get("protect", {})
            for field, key in (("attack_ms", "attack"), ("release_ms", "release")):
                if field not in protect:
                    continue
                pair = limits.get(key)
                if pair is not None and not pair[0] <= protect[field] <= pair[1]:
                    errs.append("E_LIMIT: output %s %s=%s outside protected bounds [%s,%s]" %
                                (out.get("id"), field, protect[field], pair[0], pair[1]))
    return errs


def schema_errors(cfg, schema):
    import jsonschema
    v = jsonschema.Draft202012Validator(schema)
    return ["E_SCHEMA: %s (at %s)" % (e.message[:80], "/".join(map(str, e.path)))
            for e in v.iter_errors(cfg)]


def check_configs():
    failures = []
    schema = load(os.path.join(ROOT, "configs", "schema.json"))
    import jsonschema
    jsonschema.Draft202012Validator.check_schema(schema)
    good = [p for p in sorted(glob.glob(os.path.join(ROOT, "configs", "*.json")))
            if os.path.basename(p) != "schema.json"]
    if not good:
        failures.append("no example configs found in configs/")
    for p in good:
        cfg = load(p)
        errs = schema_errors(cfg, schema) or semantic_errors(cfg)
        if errs:
            failures.append("%s: %s" % (os.path.relpath(p, ROOT), "; ".join(errs[:4])))
        else:
            print("  ok   %s" % os.path.relpath(p, ROOT))
    bad = sorted(glob.glob(os.path.join(ROOT, "configs", "invalid", "*.json")))
    for p in bad:
        cfg = load(p)
        expect = cfg.pop("_expect", None)
        test_protected = {
            "out_ids": cfg.pop("_test_protected_ids", None),
            "tdm_slot_count": cfg.pop("_test_tdm_slot_count", None),
            "lim_bounds": cfg.pop("_test_lim_bounds", {})
        }
        errs = schema_errors(cfg, schema)
        if not errs:
            errs += semantic_errors(cfg, test_protected)
        codes = {e.split(":")[0] for e in errs}
        name = os.path.relpath(p, ROOT)
        if expect is None:
            failures.append("%s: missing _expect" % name)
        elif expect not in codes:
            failures.append("%s: expected %s, got %s" % (name, expect, sorted(codes) or "no error"))
        else:
            print("  ok   %s rejected with %s" % (name, expect))
    return failures

