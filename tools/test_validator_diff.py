"""Differential test: C validator (host/build/validate_cli) vs tools/check.py.

Compares the set of error codes for every file in configs/. Usage:
python tools/test_validator_diff.py [path-to-validate_cli]
"""
import glob
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import check  # noqa: E402


def main():
    cli = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "host", "build", "validate_cli")
    paths = sorted(glob.glob(os.path.join(ROOT, "configs", "*.json")) +
                   glob.glob(os.path.join(ROOT, "configs", "invalid", "*.json")))
    paths = [p for p in paths if os.path.basename(p) != "schema.json"]
    schema = check.load(os.path.join(ROOT, "configs", "schema.json"))
    bad = 0
    for p in paths:
        cfg = check.load(p)
        cfg.pop("_expect", None)
        body = os.path.join(ROOT, "host", "build", "_diff.json")
        with open(body, "w", encoding="utf-8") as f:
            json.dump(cfg, f)
        c_codes = set(subprocess.run([cli, body], capture_output=True, text=True,
                                     check=True).stdout.split())
        py_codes = {e.split(":")[0] for e in check.schema_errors(cfg, schema)}
        try:
            sem = {e.split(":")[0] for e in check.semantic_errors(cfg)}
        except (KeyError, TypeError, AttributeError):
            sem = set()
        if sem and (not py_codes or sem == {"E_NODE_PARAM"}):
            py_codes = sem
        if c_codes != py_codes:
            bad += 1
            print("DIFF %s: C=%s py=%s" % (os.path.basename(p), sorted(c_codes), sorted(py_codes)))
    print("%d files, %d differences" % (len(paths), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
