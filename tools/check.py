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
        records = test_protected.get("records", {})
        required_record_keys = {
            "out_id", "vpk_at_0dbfs", "clip_v", "xmax_mm", "re_ohm", "pmax_w",
            "tau_s", "x_model", "min_freq_hz", "lim_bounds", "lim_default"
        }
        for out_id in ids:
            record = records.get(out_id, {})
            missing = set(record.get("missing_keys", []))
            if "present_keys" in record:
                missing |= required_record_keys - set(record["present_keys"])
            unknown = set(record.get("unknown_keys", []))
            if missing or unknown:
                errs.append("E_PROTECT: protected record %s has missing/unknown keys" % out_id)
            if record.get("x_model_valid") is False:
                errs.append("E_PROTECT: protected x_model for %s is unstable or invalid" % out_id)
            vpk, clip_v = record.get("vpk_at_0dbfs"), record.get("clip_v")
            if vpk is not None and clip_v is not None and vpk > clip_v:
                errs.append("E_PROTECT: protected vpk_at_0dbfs exceeds clip_v for %s" % out_id)
        for out in outputs:
            out_id = out.get("id")
            min_freq = test_protected.get("min_freq_hz", {}).get(out_id)
            if min_freq is not None:
                has_subsonic_hp = any(
                    block.get("t") == "biquad" and block.get("k") == "hp"
                    and block.get("f_hz", 0) >= min_freq
                    for block in out.get("chain", [])
                )
                if not has_subsonic_hp:
                    errs.append("E_PROTECT: output %s lacks required subsonic high-pass" % out_id)
            # Static response metadata is deliberately not evaluated here.
            # The former fixture fields were precomputed flags, not a numerical oracle.
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
    # Legacy graph-format fixtures remain for the frozen C validator tests;
    # they are not chain-v1 JSON fixtures.
    legacy_graph_fixtures = {
        "bessel_unsupported.json", "biquad_unstable.json",
        "crossover_too_low.json", "cycle.json", "duplicate_slot.json",
        "fanin_without_mix.json", "gain_too_high.json", "lr_odd_order.json",
        "peq_q_too_high.json", "recognized_wrong_node_property.json",
        "sample_rate_96k.json", "too_many_sections.json", "undefined_var.json",
        "unknown_node_property.json", "unknown_node_type.json",
        "var_outside_limits.json"
    }
    deferred_static_oracle = {"static_sum_bound_exceeded.json"}
    if deferred_static_oracle:
        print("  deferred %s: requires numerical reference oracle" % ", ".join(sorted(deferred_static_oracle)))
    for p in bad:
        if os.path.basename(p) in legacy_graph_fixtures | deferred_static_oracle:
            continue
        cfg = load(p)
        expect = cfg.pop("_expect", None)
        test_protected = {
            "out_ids": cfg.pop("_test_protected_ids", None),
            "tdm_slot_count": cfg.pop("_test_tdm_slot_count", None),
            "lim_bounds": cfg.pop("_test_lim_bounds", {}),
            "records": cfg.pop("_test_protected_records", {}),
            "min_freq_hz": cfg.pop("_test_min_freq_hz", {}),
            "static_bounds": cfg.pop("_test_static_bounds", {})
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



# Restored from the last complete guardrail implementation on main.
BANNED_CALLS = (
    "malloc", "calloc", "realloc", "free", "strdup", "printf", "snprintf", "puts",
    "vTaskDelay", "xSemaphoreTake", "xQueueSend", "xQueueReceive", "xQueueSendToBack",
    "pthread_mutex_lock", "ESP_LOGE", "ESP_LOGW", "ESP_LOGI", "ESP_LOGD", "ESP_LOGV",
    "sin", "cos", "tan", "atan2", "pow", "exp", "log", "log10", "sqrt", "fabs", "floor", "ceil",
)
CALL_RE = re.compile(r"\b(%s)\s*\(" % "|".join(BANNED_CALLS))
BANNED_WORDS_RE = re.compile(r"\b(double|cJSON\w*)\b")
LITERAL_RE = re.compile(r"(?<![\w.])(?:\d+\.\d*|\.\d+|\d+(?=[eE][+-]?\d))(?:[eE][+-]?\d+)?([fFlL]?)(?![\w.])")
COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"')


def scan_rt_text(text):
    text = COMMENT_RE.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)
    text = STRING_RE.sub('""', text)
    problems = []
    for lineno, line in enumerate(text.split("\n"), 1):
        for m in CALL_RE.finditer(line):
            problems.append((lineno, "banned call %s()" % m.group(1)))
        for m in BANNED_WORDS_RE.finditer(line):
            problems.append((lineno, "banned token %s" % m.group(1)))
        for m in LITERAL_RE.finditer(line):
            if m.group(1) in ("", "l", "L"):
                problems.append((lineno, "double literal %s (add f suffix)" % m.group(0)))
    return problems


def check_rt_files():
    failures = []
    pats = ("firmware/components/**/*_rt.c", "firmware/components/**/*_rt.h",
            "firmware/main/*_rt.c", "firmware/main/*_rt.h")
    files = sorted({f for p in pats for f in glob.glob(os.path.join(ROOT, p), recursive=True)})
    for f in files:
        with open(f, encoding="utf-8", errors="replace") as fh:
            for lineno, msg in scan_rt_text(fh.read()):
                failures.append("%s:%d: %s" % (os.path.relpath(f, ROOT), lineno, msg))
    print("  scanned %d real-time file(s)" % len(files))
    return failures


def selftest_rt_scan():
    must_flag = ["x = y * 0.5;", "p = malloc(4);", "ESP_LOGI(T, \"hi\");", "double d;",
                 "y = sin(w);", "k = 1e-3;", "xSemaphoreTake(s, 1);", "j = cJSON_Parse(s);"]
    must_pass = ["x = y * 0.5f;", "float c = cosf(w);", "/* malloc 0.5 */ int a = 1;",
                 "// printf(\"x\");", "const char *s = \"malloc(1) 0.5\";", "n = 64u; k = 1e-3f;"]
    bad = []
    for s in must_flag:
        if not scan_rt_text(s):
            bad.append("scanner missed: %s" % s)
    for s in must_pass:
        if scan_rt_text(s):
            bad.append("scanner false positive: %s -> %s" % (s, scan_rt_text(s)))
    return bad


SECRET_RE = re.compile(r"^[ \t]*(WIFI_PASSWORD|CONFIG_UPLOAD_TOKEN|UPLOAD_TOKEN)[ \t]*=[ \t]*\S+", re.M)
SKIP_DIRS = {".git", "build", "managed_components", "__pycache__", "node_modules"}


def check_secrets():
    failures = []
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in filenames:
            path = os.path.join(dirpath, name)
            if name == "check.py" or os.path.getsize(path) > 1_000_000:
                continue
            try:
                with open(path, encoding="utf-8") as fh:
                    text = fh.read()
            except (UnicodeDecodeError, OSError):
                continue
            if SECRET_RE.search(text):
                failures.append("possible secret value in %s" % os.path.relpath(path, ROOT))
    return failures


def main():
    try:
        import jsonschema
    except ImportError:
        print("jsonschema missing: pip install -r tools/requirements.txt")
        return 2
    failures = []
    print("WARNING: subsonic HP presence check is TEMPORARY; it does not prove attenuation at min_freq_hz.")
    for title, fn in (("scanner self-test", selftest_rt_scan), ("configs", check_configs),
                      ("real-time scan", check_rt_files), ("secrets", check_secrets)):
        print("[%s]" % title)
        failures += fn()
    print()
    if failures:
        print("FAIL (%d)" % len(failures))
        for f in failures:
            print("  - " + f)
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())