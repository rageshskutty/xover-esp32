#!/usr/bin/env python3
"""Repository guardrails for the frozen parametric-filter configuration contract."""
import glob
import json
import math
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
# Total filters across pre-chains, output chains and detector weighting. Re-derive from bench CPU data.
MAX_BIQUADS = 96

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
    biquads = sum(len(x.get("filters", [])) for x in pre) + sum(len(x.get("filters", [])) for x in outputs)
    biquads += sum(len(r.get("weighting", {}).get("filters", []))
                   for r in cfg.get("protection", {}).get("outputs", []))
    if biquads > MAX_BIQUADS:
        errs.append("E_RESOURCE: %d filters exceed the %d biquad budget" % (biquads, MAX_BIQUADS))
    seen_linked = set()
    for group in cfg.get("links", []):
        for oid in group:
            if oid not in ids:
                errs.append("E_PROTECT: link references unknown output %r" % oid)
            if oid in seen_linked:
                errs.append("E_PROTECT: output %r appears in more than one link slot" % oid)
            seen_linked.add(oid)
    protection=cfg.get("protection")
    if protection is not None:
        for rec in protection.get("outputs",[]):
            taus=[st.get("rms_tau_ms") for st in rec.get("thermal",[])]
            if taus!=sorted(set(taus)):
                errs.append("E_PROTECT: output %s thermal rms_tau_ms must be strictly increasing" % rec.get("id"))
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


ADD_TEST_RE = re.compile(r"add_test\(\s*NAME\s+(\w+)")


def planning_errors(state, cmake_text, test_files, existing_paths):
    """Pure rule: every host test is owned by a task or explicitly deferred, and nothing is orphaned.

    state: parsed task_state.json; cmake_text: host/CMakeLists.txt; test_files: file names under
    host/tests (top level, test_*.c); existing_paths: set of repo-relative paths that exist.
    """
    errs = []
    if not isinstance(state, dict) or not isinstance(state.get("tasks"), list):
        return ["task_state.json: must be an object with a 'tasks' list"]
    ids, owned = set(), set()
    for task in state["tasks"]:
        tid = task.get("id") if isinstance(task, dict) else None
        if not tid or not task.get("state"):
            errs.append("task_state.json: every task needs id and state (got %r)" % (task,))
            continue
        if tid in ids:
            errs.append("task_state.json: duplicate task id %s" % tid)
        ids.add(tid)
        tests = task.get("tests", [])
        if not isinstance(tests, list) or not all(isinstance(t, str) for t in tests):
            errs.append("task_state.json: %s tests must be a list of strings" % tid)
            continue
        for t in tests:
            if t in owned:
                errs.append("task_state.json: test %s is owned by more than one task" % t)
            owned.add(t)
    deferred = set()
    for d in state.get("deferred_tests", []):
        path = d.get("path") if isinstance(d, dict) else None
        if not path or not d.get("reason"):
            errs.append("task_state.json: deferred test needs path and reason (got %r)" % (d,))
            continue
        deferred.add(path)
        if path not in existing_paths:
            errs.append("task_state.json: deferred test %s does not exist" % path)
    registered = set(ADD_TEST_RE.findall(cmake_text))
    for name in sorted(registered):
        if name not in owned:
            errs.append("orphan test: %s is built in host/CMakeLists.txt but owned by no task" % name)
    for name in sorted(owned):
        if name not in registered:
            errs.append("task_state.json: test %s is not registered in host/CMakeLists.txt" % name)
    for fname in sorted(test_files):
        stem = os.path.splitext(fname)[0]
        if stem not in registered and "host/tests/" + fname not in deferred:
            errs.append("orphan test: host/tests/%s is neither built nor listed in deferred_tests" % fname)
    return errs


def check_planning():
    state_path = os.path.join(ROOT, "docs", "engineering", "task_state.json")
    try:
        state = load(state_path)
    except (OSError, ValueError) as exc:
        return ["docs/engineering/task_state.json: %s" % exc]
    with open(os.path.join(ROOT, "host", "CMakeLists.txt"), encoding="utf-8") as fh:
        cmake_text = fh.read()
    tests_dir = os.path.join(ROOT, "host", "tests")
    test_files = [f for f in os.listdir(tests_dir) if re.fullmatch(r"test_\w+\.c", f)]
    existing = {"host/tests/" + f for f in os.listdir(tests_dir)}
    errs = planning_errors(state, cmake_text, test_files, existing)
    if not errs:
        print("  ok   %d task(s), %d test file(s) accounted for" % (len(state["tasks"]), len(test_files)))
    return errs


def selftest_planning():
    good = {"tasks": [{"id": "A", "state": "merged", "tests": ["test_a"]}],
            "deferred_tests": [{"path": "host/tests/test_b.c", "reason": "x"}]}
    cmake = "add_test(NAME test_a COMMAND test_a)"
    files = ["test_a.c", "test_b.c"]
    paths = {"host/tests/test_a.c", "host/tests/test_b.c"}
    bad = []
    if planning_errors(good, cmake, files, paths):
        bad.append("planning rule false positive on a consistent state")
    if not planning_errors(good, cmake, files + ["test_c.c"], paths | {"host/tests/test_c.c"}):
        bad.append("planning rule missed an unbuilt, undeferred test file")
    if not planning_errors(good, cmake + "\nadd_test(NAME test_d COMMAND test_d)", files, paths):
        bad.append("planning rule missed a built test that no task owns")
    if not planning_errors({"tasks": []}, cmake, files, paths):
        bad.append("planning rule missed a task list that owns no test")
    return bad


def main():
    try:
        import jsonschema
    except ImportError:
        print("jsonschema missing: pip install -r tools/requirements.txt")
        return 2
    failures = []
    for title, fn in (("scanner self-test", selftest_rt_scan), ("planning self-test", selftest_planning),
                      ("configs", check_configs), ("planning integrity", check_planning),
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
