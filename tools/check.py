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


def semantic_errors(cfg):
    errs = []
    nodes = cfg.get("nodes", [])
    edges = cfg.get("edges", [])
    variables = cfg.get("vars", {})
    limits = cfg.get("limits", {})
    by_id = {}
    if cfg.get("sample_rate_hz") != FS:
        errs.append("E_SCHEMA: sample_rate_hz must be %d" % FS)
    if len(nodes) > MAX_NODES:
        errs.append("E_RESOURCE: %d nodes > %d" % (len(nodes), MAX_NODES))
    for n in nodes:
        if n["id"] in by_id:
            errs.append("E_GRAPH_REF: duplicate id %s" % n["id"])
        by_id[n["id"]] = n
    for name, lim in limits.items():
        if name not in variables:
            errs.append("E_VAR_UNDEFINED: limit for unknown var %s" % name)
            continue
        if lim["min"] >= lim["max"]:
            errs.append("E_LIMIT: %s min >= max" % name)
        if not lim["min"] <= variables[name] <= lim["max"]:
            errs.append("E_LIMIT: var %s=%s outside [%s, %s]" %
                        (name, variables[name], lim["min"], lim["max"]))
    incoming, outgoing = defaultdict(list), defaultdict(list)
    for a, b in edges:
        if a not in by_id or b not in by_id or a == b:
            errs.append("E_GRAPH_REF: bad edge %s -> %s" % (a, b))
            continue
        outgoing[a].append(b)
        incoming[b].append(a)
    indeg = {i: len(incoming[i]) for i in by_id}
    queue = [i for i, d in indeg.items() if d == 0]
    seen = 0
    while queue:
        x = queue.pop()
        seen += 1
        for y in outgoing[x]:
            indeg[y] -= 1
            if indeg[y] == 0:
                queue.append(y)
    if seen != len(by_id):
        errs.append("E_GRAPH_CYCLE: graph is not a DAG")
    slots, channels = [], []
    total_sections = 0
    for n in nodes:
        t, i = n["type"], n["id"]
        total_sections += sections(n)
        for fld in NUMERIC_FIELDS:
            if fld in n:
                val, undefined = resolve(n[fld], variables)
                if undefined:
                    errs.append("E_VAR_UNDEFINED: %s.%s uses @%s" % (i, fld, undefined))
                elif isinstance(n[fld], str) and val is None:
                    errs.append("E_NODE_PARAM: %s.%s is not a number or @var" % (i, fld))
                elif val is not None and fld == "frequency_hz" and not 10 <= val <= 0.45 * FS:
                    errs.append("E_NODE_PARAM: %s.frequency_hz %s outside 10..%.0f" %
                                (i, val, 0.45 * FS))
        if t == "input":
            if incoming[i]:
                errs.append("E_GRAPH_FANIN: input %s has an incoming edge" % i)
            channels.append(n.get("channel"))
        elif t == "mix":
            if len(incoming[i]) < 2 or len(incoming[i]) != len(n.get("gains", [])):
                errs.append("E_GRAPH_FANIN: mix %s needs one gain per incoming edge (>=2)" % i)
        elif len(incoming[i]) != 1:
            errs.append("E_GRAPH_FANIN: %s (%s) must have exactly 1 incoming edge, has %d" %
                        (i, t, len(incoming[i])))
        if t == "output":
            slots.append(n.get("slot"))
            if outgoing[i]:
                errs.append("E_GRAPH_DEAD: output %s has an outgoing edge" % i)
        elif not outgoing[i]:
            errs.append("E_GRAPH_DEAD: %s (%s) has no outgoing edge" % (i, t))
        for fld, lo, hi, types in (("q", 0.1, 20, ("peq", "allpass", "notch")),
                                   ("db", -24, 12, ("peq", "shelf")),
                                   ("db", -60, 12, ("gain",))):
            if t in types and fld in n:
                val, _ = resolve(n[fld], variables)
                if val is not None and not lo <= val <= hi:
                    errs.append("E_NODE_PARAM: %s.%s %s outside %s..%s" %
                                (i, fld, val, lo, hi))
        if t == "crossover":
            fam, order = n.get("family"), n.get("order")
            if fam == "Bessel":
                errs.append("E_UNSUPPORTED: %s Bessel normalisation undecided (design.md section 3)" % i)
            elif fam not in FAMILIES:
                errs.append("E_NODE_PARAM: %s family %r not in %s" % (i, fam, FAMILIES))
            elif fam == "Linkwitz-Riley" and order not in (2, 4, 6, 8):
                errs.append("E_NODE_PARAM: %s Linkwitz-Riley order %r must be 2,4,6 or 8" % (i, order))
            elif fam != "Linkwitz-Riley" and not (isinstance(order, int) and 1 <= order <= 8):
                errs.append("E_NODE_PARAM: %s order %r must be 1..8" % (i, order))
            if "frequency_hz" not in n or "mode" not in n:
                errs.append("E_NODE_PARAM: %s needs mode and frequency_hz" % i)
        elif t == "peq" and not all(k in n for k in ("frequency_hz", "q", "db")):
            errs.append("E_NODE_PARAM: peq %s needs frequency_hz, q, db" % i)
        elif t == "shelf" and not (all(k in n for k in ("frequency_hz", "db")) and n.get("shape") in ("low", "high")):
            errs.append("E_NODE_PARAM: shelf %s needs frequency_hz, db, shape low|high" % i)
        elif t in ("allpass", "notch") and not all(k in n for k in ("frequency_hz", "q")):
            errs.append("E_NODE_PARAM: %s %s needs frequency_hz, q" % (t, i))
        elif t == "biquad":
            b, a = n.get("b"), n.get("a")
            ok = (isinstance(b, list) and len(b) == 3 and isinstance(a, list) and len(a) == 2
                  and n.get("sample_rate_hz") == FS)
            if not ok:
                errs.append("E_NODE_PARAM: biquad %s needs b[3], a[2], sample_rate_hz=%d" % (i, FS))
            elif not biquad_stable(b, a):
                errs.append("E_NODE_PARAM: biquad %s is unstable or not finite float32" % i)
        elif t == "gain" and "db" not in n:
            errs.append("E_NODE_PARAM: gain %s needs db" % i)
        elif t == "delay":
            d = n.get("delay_us")
            if not isinstance(d, (int, float)) or not 0 <= d <= MAX_DELAY_US:
                errs.append("E_NODE_PARAM: delay %s needs delay_us in 0..%d" % (i, MAX_DELAY_US))
    if len(slots) != len(set(slots)) or any(
            not isinstance(s, int) or not 0 <= s < N_SLOTS for s in slots):
        errs.append("E_SLOT: output slots must be unique integers 0..%d" % (N_SLOTS - 1))
    if len(channels) != len(set(channels)):
        errs.append("E_SLOT: duplicate input channel")
    if not slots:
        errs.append("E_GRAPH_DEAD: no output node")
    if total_sections > MAX_SECTIONS:
        errs.append("E_RESOURCE: %d biquad sections > %d" % (total_sections, MAX_SECTIONS))
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
        # Schema failures use E_SCHEMA; semantic fixtures also check that a
        # range failure retains its public E_NODE_PARAM classification.
        errs = schema_errors(cfg, schema)
        if expect != "E_SCHEMA":
            errs += semantic_errors(cfg)
        codes = {e.split(":")[0] for e in errs}
        name = os.path.relpath(p, ROOT)
        if expect is None:
            failures.append("%s: missing _expect" % name)
        elif expect not in codes:
            failures.append("%s: expected %s, got %s" % (name, expect, sorted(codes) or "no error"))
        else:
            print("  ok   %s rejected with %s" % (name, expect))
    return failures


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
