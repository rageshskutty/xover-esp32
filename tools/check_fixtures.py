#!/usr/bin/env python3
"""Compile-check committed host test fixtures that CMake does not register yet.

Planning PRs commit fixtures before the production headers exist, so CI never
compiled them. A fixture that does not compile hands a 7B executor a broken
test and invites it to bend correct code. This syntax-checks every
unregistered host/tests/*.c with the same warning flags as host/CMakeLists.txt.

Result per fixture:
  OK       compiles (real header, or a stub from host/tests/stubs)
  PENDING  only fails because a planned header does not exist and has no stub,
           or its first error is an undeclared identifier that docs/ documents
           as planned API (later errors in that file stay masked until the
           symbol lands; this is a known limitation)
  FAIL     any other compile error (exit status 1)
Real include directories come first and stubs last, so a real header wins as
soon as the task that creates it lands.
"""
import glob
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FLAGS = ["-std=c11", "-fsyntax-only", "-Wall", "-Wextra", "-Wconversion", "-Werror"]


def registered():
    text = open(os.path.join(ROOT, "host", "CMakeLists.txt"), encoding="utf-8").read()
    return set(re.findall(r"tests/([A-Za-z0-9_]+\.c)", text))


def docs_text():
    chunks = []
    for p in glob.glob(os.path.join(ROOT, "docs", "**", "*.md"), recursive=True):
        chunks.append(open(p, encoding="utf-8", errors="replace").read())
    return "\n".join(chunks)


def planned_symbol(stderr, docs):
    for line in stderr.splitlines():
        if " error: " not in line:
            continue
        m = (re.search(r"'(\w+)' undeclared", line) or
             re.search(r"unknown type name '?(\w+)'?", line) or
             re.search(r"implicit declaration of function '?(\w+)'?", line))
        if m and re.search(r"\b%s\b" % re.escape(m.group(1)), docs):
            return m.group(1)
        return None
    return None


def include_dirs():
    dirs = []
    for pat in ("firmware/components/*/include", "firmware/components/*/internal",
                "firmware/third_party/cjson", "host/tests/golden"):
        dirs += sorted(glob.glob(os.path.join(ROOT, pat)))
    stubs = os.path.join(ROOT, "host", "tests", "stubs")
    if os.path.isdir(stubs):
        dirs.append(stubs)
    return ["-I" + d for d in dirs]


def main():
    cc = shutil.which("gcc") or shutil.which("cc") or shutil.which("clang")
    if not cc:
        print("check_fixtures: no C compiler found", file=sys.stderr)
        return 2
    skip = registered()
    fixtures = sorted(p for p in glob.glob(os.path.join(ROOT, "host", "tests", "*.c"))
                      if os.path.basename(p) not in skip)
    docs = docs_text()
    fail = 0
    counts = {"OK": 0, "PENDING": 0, "FAIL": 0}
    for path in fixtures:
        name = os.path.relpath(path, ROOT)
        r = subprocess.run([cc, *FLAGS, *include_dirs(), path],
                           capture_output=True, text=True)
        if r.returncode == 0:
            status, note = "OK", ""
        else:
            m = re.search(r"fatal error: ([^:\s]+): No such file", r.stderr)
            if m:
                status, note = "PENDING", "planned header %s has no stub" % m.group(1)
            else:
                sym = planned_symbol(r.stderr.replace("\u2018", "'").replace("\u2019", "'"), docs)
                if sym:
                    status, note = "PENDING", "planned symbol %s not declared yet" % sym
                else:
                    status, note = "FAIL", "\n" + r.stderr.strip()
        counts[status] += 1
        print("%-8s %s %s" % (status, name, note))
        if status == "FAIL":
            fail = 1
    print("fixtures: %d unregistered; ok=%d pending=%d fail=%d" %
          (len(fixtures), counts["OK"], counts["PENDING"], counts["FAIL"]))
    return fail


if __name__ == "__main__":
    sys.exit(main())
