#!/usr/bin/env python3
"""Keeps README.md honest, so the documentation can't drift away from the code.

  1. Every complete program in the README (a C block that starts with
     `#define TIC_IMPLEMENTATION` and has a main) must compile without warnings.
  2. Every TIC name the README mentions must exist in tic.h.

Usage: python3 tests/check_readme.py [compiler]
"""

import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
compiler = sys.argv[1] if len(sys.argv) > 1 else "cc"

with open(os.path.join(ROOT, "README.md"), encoding="utf-8") as f:
    readme = f.read()
with open(os.path.join(ROOT, "tic.h"), encoding="utf-8") as f:
    header = f.read()

failures = 0

blocks = re.findall(r"```c\n(.*?)```", readme, re.S)
programs = [b for b in blocks if b.startswith("#define TIC_IMPLEMENTATION") and "int main" in b]
if not programs:
    print("no complete programs found in README.md")
    failures += 1

with tempfile.TemporaryDirectory() as folder:
    for i, program in enumerate(programs):
        path = os.path.join(folder, "readme_%d.c" % i)
        with open(path, "w", encoding="utf-8") as f:
            f.write(program)
        result = subprocess.run(
            [compiler, "-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-I", ROOT, path,
             "-o", os.path.join(folder, "readme_%d" % i)],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            print("README program %d does not compile:\n%s" % (i + 1, result.stderr))
            failures += 1

names = set(re.findall(r"\b(?:tic|TIC)_[A-Za-z0-9_]+\b", readme))
for name in sorted(names):
    if not re.search(r"\b%s\b" % re.escape(name), header):
        print("README mentions %s, which isn't in tic.h" % name)
        failures += 1

print("%s: %d programs compiled, %d names checked, %d problems"
      % ("FAILED" if failures else "ok", len(programs), len(names), failures))
sys.exit(1 if failures else 0)
