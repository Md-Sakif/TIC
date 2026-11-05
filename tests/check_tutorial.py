#!/usr/bin/env python3
"""Keeps the tutorials honest, so what they show can't drift away from the real code.

A code block in a tutorial is checked if a comment names its file just above it:

    <!-- file: step3.c -->
    ```c
    ...code...
    ```

That exact code has to appear in that file, which is compiled and run by the tests.

It also checks that:
  - every step program is used by its tutorial
  - every relative link goes to a file that exists
  - every TIC name a tutorial mentions exists in tic.h

Usage: python3 tests/check_tutorial.py
"""

import glob
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
problems = []
checked_blocks = 0

with open(os.path.join(ROOT, "tic.h"), encoding="utf-8") as f:
    header = f.read()
with open(os.path.join(ROOT, "tutorial", "system-monitor", "stats.h"), encoding="utf-8") as f:
    stats_header = f.read()

documents = glob.glob(os.path.join(ROOT, "tutorial", "**", "*.md"), recursive=True)
if not documents:
    problems.append("no tutorials found")

for document in documents:
    name = os.path.relpath(document, ROOT)
    folder = os.path.dirname(document)
    with open(document, encoding="utf-8") as f:
        text = f.read()

    for source, block in re.findall(r"<!-- file: (\S+) -->\n```c\n(.*?)```", text, re.S):
        checked_blocks += 1
        path = os.path.join(folder, source)
        if not os.path.exists(path):
            problems.append("%s names %s, which doesn't exist" % (name, source))
            continue
        with open(path, encoding="utf-8") as f:
            if block not in f.read():
                problems.append("%s: this code isn't in %s:\n%s" % (name, source, block))

    for target in re.findall(r"\]\(([^)#\s]+)(?:#[^)]*)?\)", text):
        if re.match(r"[a-z]+:", target):
            continue
        if not os.path.exists(os.path.join(folder, target)):
            problems.append("%s links to %s, which doesn't exist" % (name, target))

    for tic_name in sorted(set(re.findall(r"\b(?:tic|TIC)_[A-Za-z0-9_]+\b", text))):
        if not re.search(r"\b%s\b" % re.escape(tic_name), header):
            problems.append("%s mentions %s, which isn't in tic.h" % (name, tic_name))

    for stats_name in sorted(set(re.findall(r"\bstats_[a-z_]+\b", text))):
        if not re.search(r"\b%s\b" % re.escape(stats_name), stats_header):
            problems.append("%s mentions %s, which isn't in stats.h" % (name, stats_name))

    for step in glob.glob(os.path.join(folder, "step*.c")):
        if os.path.basename(step) not in text:
            problems.append("%s never mentions %s" % (name, os.path.basename(step)))

for problem in problems:
    print(problem)
print("%s: %d documents, %d code blocks checked, %d problems"
      % ("FAILED" if problems else "ok", len(documents), checked_blocks, len(problems)))
sys.exit(1 if problems else 0)
