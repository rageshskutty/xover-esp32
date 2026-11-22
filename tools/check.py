#!/usr/bin/env python3
"""Repository guardrails for the frozen parametric-filter configuration contract."""
import glob
import json
import math
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

def load(path):
    def reject_constant(value):
        raise ValueError("non-finite JSON number: " + value)
    def finite_float(value):
        number = float(value)
        if not math.isfinite(number):
            raise ValueError("non-finite JSON number: " + value)
        return number
    with open(path, encoding="utf-8") as f:
        return json.load(f, parse_constant=reject_constant, parse_float=finite_float)

def schema_errors(cfg, schema):
    import jsonschema
    return ["E_SCHEMA: %s (at %s)" % (e.message[:100], "/".join(map(str, e.path)))
            for e in jsonschema.Draft202012Validator(schema).iter_errors(cfg)]

def semantic_errors(cfg):
    errs=[]
    inputs=cfg.get("inputs",[])
    declared=set(inputs)
    pre=cfg.get("pre",[])
    pre_channels=[x.get("channel") for x in pre]
    if len(pre_channels)!=len(set(pre_channels)):
        errs.append("E_SCHEMA: pre channel must be unique")
    for ch in pre_channels:
        if ch not in declared:
            errs.append("E_SCHEMA: pre channel %r is not declared in inputs" % ch)
    outputs=cfg.get("outputs",[])
    ids=[x.get("id") for x in outputs]
    slots=[x.get("slot") for x in outputs]
    if len(ids)!=len(set(ids)):
        errs.append("E_SCHEMA: output id must be unique")
    if len(slots)!=len(set(slots)):
        errs.append("E_SCHEMA: output slot must be unique")
    for out in outputs:
        seen=set()
        for mix in out.get("in",[]):
            ch=mix.get("channel")
            if ch not in declared:
                errs.append("E_SCHEMA: output %s references undeclared input %r" % (out.get("id"),ch))
            if ch in seen:
                errs.append("E_SCHEMA: output %s repeats input channel %r" % (out.get("id"),ch))
            seen.add(ch)
    protection=cfg.get("protection")
    if protection is not None:
        records=protection.get("outputs",[])
        record_ids=[x.get("id") for x in records]
        if len(record_ids)!=len(set(record_ids)):
            errs.append("E_PROTECT: protection output id must be unique")
        if set(record_ids)!=set(ids):
            errs.append("E_PROTECT: protection records must cover configured outputs exactly")
    return errs

def check_configs():
    failures=[]
    schema=load(os.path.join(ROOT,"configs","schema.json"))
    import jsonschema
    jsonschema.Draft202012Validator.check_schema(schema)
    good=[p for p in sorted(glob.glob(os.path.join(ROOT,"configs","*.json"))) if os.path.basename(p)!="schema.json"]
    if not good: failures.append("no example configs found in configs/")
    for path in good:
        try:
            cfg=load(path)
        except (ValueError, json.JSONDecodeError) as exc:
            failures.append("%s: E_SCHEMA: %s" % (os.path.relpath(path,ROOT), exc))
            continue
        errs=schema_errors(cfg,schema)
        if not errs: errs=semantic_errors(cfg)
        if errs: failures.append("%s: %s" % (os.path.relpath(path,ROOT),"; ".join(errs[:4])))
        else: print("  ok   %s" % os.path.relpath(path,ROOT))
    bad=sorted(glob.glob(os.path.join(ROOT,"configs","invalid","*.json")))
    if not bad: failures.append("no invalid configuration fixtures found")
    for path in bad:
        name=os.path.relpath(path,ROOT)
        try:
            with open(path, encoding="utf-8") as f:
                raw=f.read()
            match=re.search(r'"_expect"\s*:\s*"(E_[A-Z]+)"', raw)
            expect=match.group(1) if match else None
            cfg=load(path)
        except (ValueError, json.JSONDecodeError):
            codes={"E_SCHEMA"}
        else:
            if cfg is not None:
                cfg.pop("_expect",None)
            errs=schema_errors(cfg,schema)
            if not errs: errs=semantic_errors(cfg)
            codes={e.split(":")[0] for e in errs}
        if expect is None: failures.append("%s: missing _expect or unreadable fixture" % name)
        elif expect not in codes: failures.append("%s: expected %s, got %s" % (name,expect,sorted(codes) or "no error"))
        else: print("  ok   %s rejected with %s" % (name,expect))
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