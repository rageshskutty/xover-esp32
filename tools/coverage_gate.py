#!/usr/bin/env python3
"""G2 gate: line coverage of firmware/components/dspblocks must be >= 90 %.

Stdlib only. Reads `gcov -n` output for the static dspblocks library objects of a
host build configured with --coverage and already run through ctest.
usage: python tools/coverage_gate.py host/build
"""
import pathlib
import re
import subprocess
import sys

GATE_PERCENT = 90.0
SOURCE_DIR = "components/dspblocks/"


def main(build_dir: str) -> int:
    build = pathlib.Path(build_dir).resolve()
    gcdas = sorted(
        str(p) for p in build.rglob("*.gcda") if "dspblocks.dir" in p.as_posix()
    )
    if not gcdas:
        print("FAIL: no .gcda files; build with --coverage and run ctest first")
        return 1
    out = subprocess.run(
        ["gcov", "-n", *gcdas], cwd=build, capture_output=True, text=True, check=True
    ).stdout
    files = {}
    for name, pct, lines in re.findall(
        r"File '([^']+)'\nLines executed:([\d.]+)% of (\d+)", out
    ):
        if SOURCE_DIR in name.replace("\\", "/"):
            files[name] = (float(pct), int(lines))
    if not files:
        print("FAIL: gcov reported no dspblocks sources")
        return 1
    # A source no test ever loads has no .gcda and would silently vanish from
    # the total, so every .c of the component must be present in the report.
    component = pathlib.Path(__file__).resolve().parent.parent / "firmware" / SOURCE_DIR
    reported = {pathlib.Path(n).name for n in files}
    missing = sorted(p.name for p in component.glob("*.c") if p.name not in reported)
    if missing:
        print(f"FAIL: no coverage data for {', '.join(missing)} (never executed by any test)")
        return 1
    total = sum(n for _, n in files.values())
    hit = sum(p / 100.0 * n for p, n in files.values())
    for name, (pct, n) in sorted(files.items()):
        print(f"  {pct:6.2f}% of {n:4d} lines  {pathlib.Path(name).name}")
    percent = 100.0 * hit / total
    print(f"dspblocks line coverage {percent:.2f}% (gate {GATE_PERCENT:.0f}%)")
    return 0 if percent >= GATE_PERCENT else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "host/build"))
