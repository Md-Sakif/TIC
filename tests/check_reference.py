#!/usr/bin/env python3
"""Keeps reference.md honest, so the API documentation can't drift away from tic.h.

It reads the public section of tic.h and fails if:

  - a public function is missing from the reference, or its signature differs
  - a public type, enum value, macro, option or struct field isn't documented
  - the reference mentions a TIC name that doesn't exist in tic.h
  - a link in the reference goes to a heading that doesn't exist
  - an example in the reference doesn't compile (blocks starting with /* Example */)

Usage: python3 tests/check_reference.py [compiler]
"""

import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
compiler = sys.argv[1] if len(sys.argv) > 1 else "cc"

with open(os.path.join(ROOT, "tic.h"), encoding="utf-8") as f:
    header = f.read()
with open(os.path.join(ROOT, "reference.md"), encoding="utf-8") as f:
    reference = f.read()

problems = []


def squeeze(text):
    """Makes two pieces of code comparable, however they're wrapped or spaced."""
    text = re.sub(r"\s+", " ", text).strip()
    text = re.sub(r"\( ", "(", text)
    return text.replace(" *", " *")


# The public section, without comments, and with include-guard lines removed.
public = header[header.index("#ifndef TIC_H"):header.index("#endif /* TIC_H */")]
code = re.sub(r"/\*.*?\*/", "", public, flags=re.S)

# Fields that exist only for the library's own use.
PRIVATE_FIELDS = {"buffer", "event", "event_used", "focus", "focus_seen", "mouse_capture", "widgets",
                  "widget_count", "current", "viewport", "saved_clip", "content_height", "bar_visible"}

# Macros that are part of how the header is built, rather than something to use.
INTERNAL_MACROS = {"TIC_H", "TIC_IMPLEMENTATION_DONE"}

# --- Functions: the exact signature has to be there --------------------------------
blocks = re.findall(r"```c\n(.*?)```", reference, re.S)
documented_code = {squeeze(b) for b in blocks}
functions = re.findall(r"^TIC_API\s+([^;#]+?\))\s*;", code, re.S | re.M)
for signature in functions:
    wanted = squeeze(signature) + ";"
    if wanted not in documented_code:
        problems.append("function missing or different in reference.md: %s" % wanted)
if not functions:
    problems.append("found no functions in tic.h, so this checker needs fixing")

# --- Every function and type has its own heading -----------------------------------
headings = re.findall(r"^#{1,6} (.+)$", reference, re.M)
heading_names = {h.strip("`") for h in headings}
function_names = [re.search(r"(\w+)\(", s).group(1) for s in functions]
type_names = re.findall(r"typedef\s+(?:struct|enum)\s+\w+\s*\{.*?\}\s*(\w+)\s*;", code, re.S)
type_names += re.findall(r"typedef\s+uint32_t\s+(\w+)\s*;", code)
type_names += re.findall(r"typedef\s+[^;]*?\(\*(\w+)\)", code)
for name in function_names + type_names:
    if name not in heading_names:
        problems.append("%s has no heading of its own in reference.md" % name)

# --- Enum values, macros and options ----------------------------------------------
names = set(re.findall(r"\bTIC_[A-Z0-9_]+\b", code))
names |= set(re.findall(r"#\s*define\s+(TIC_\w+)", header[:header.index("* IMPLEMENTATION")]))
names |= {"TIC_IMPLEMENTATION", "TIC_STATIC", "TIC_API", "TIC_NO_UI", "TIC_MALLOC", "TIC_REALLOC", "TIC_FREE",
          "TIC_ESC_TIMEOUT_MS", "TIC_PASTE_TIMEOUT_MS", "TIC_PASTE_MAX"}
names -= INTERNAL_MACROS
for name in sorted(names):
    if "`%s" % name not in reference and "%s`" % name not in reference and "[%s" % name not in reference:
        problems.append("%s isn't documented in reference.md" % name)

# --- Struct fields ----------------------------------------------------------------
for fields, type_name in re.findall(r"typedef\s+struct\s+\w+\s*\{(.*?)\}\s*(\w+)\s*;", code, re.S):
    section = re.search(r"^### `%s`\n(.*?)(?=^#{2,3} )" % type_name, reference + "\n### end", re.S | re.M)
    if not section:
        continue
    for declaration in fields.split(";"):
        for field in re.findall(r"(\w+)(?:\[[^\]]*\])*\s*(?:,|$)", declaration.strip()):
            if field in PRIVATE_FIELDS or type_name == "tic_ui_slot":
                continue
            if "`%s`" % field not in section.group(1):
                problems.append("%s.%s isn't described in reference.md" % (type_name, field))

# --- Names in the reference have to be real ----------------------------------------
# Link targets are skipped, because headings turn into lowercase ones such as #tic_rgb.
without_links = re.sub(r"\]\(#[^)]*\)", "]", reference)
for name in sorted(set(re.findall(r"\b(?:tic|TIC)_[A-Za-z0-9_]+\b", without_links))):
    if not re.search(r"\b%s\b" % re.escape(name), header):
        problems.append("reference.md mentions %s, which isn't in tic.h" % name)


# --- Links ------------------------------------------------------------------------
def slug(heading):
    """How GitHub turns a heading into the link target for it."""
    text = heading.replace("`", "").strip().lower()
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")


anchors = set()
seen = {}
for heading in headings:
    base = slug(heading)
    count = seen.get(base, 0)
    anchors.add(base if count == 0 else "%s-%d" % (base, count))
    seen[base] = count + 1

outside_code = re.sub(r"```.*?```", "", reference, flags=re.S)
for target in re.findall(r"\]\(([^)\s]+)\)", outside_code):
    if target.startswith("#"):
        if target[1:] not in anchors:
            problems.append("reference.md links to %s, which isn't a heading" % target)
    elif not re.match(r"[a-z]+:", target) and not os.path.exists(os.path.join(ROOT, target)):
        problems.append("reference.md links to %s, which doesn't exist" % target)

# --- Examples have to compile ------------------------------------------------------
PRELUDE = """#define TIC_IMPLEMENTATION
#include "tic.h"
static tic_buffer *screen;
static tic_style bold;
static tic_rect panel, area;
static tic_event event;
static tic_ui ui;
static tic_input name;
static tic_scroll scroll;
"""
examples = [b for b in blocks if b.startswith("/* Example */")]
if not examples:
    problems.append("found no examples in reference.md, so this checker needs fixing")
with tempfile.TemporaryDirectory() as folder:
    path = os.path.join(folder, "examples.c")
    with open(path, "w", encoding="utf-8") as f:
        f.write(PRELUDE)
        for i, example in enumerate(examples):
            f.write("void example_%d(void);\nvoid example_%d(void) {\n%s}\n" % (i, i, example))
        f.write("int main(void) { (void)panel; (void)area; (void)bold; (void)name; return 0; }\n")
    result = subprocess.run(
        [compiler, "-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-I", ROOT, path,
         "-o", os.path.join(folder, "examples")],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        problems.append("an example in reference.md doesn't compile:\n%s" % result.stderr)

for problem in problems:
    print(problem)
print("%s: %d functions, %d types, %d other names, %d examples checked, %d problems"
      % ("FAILED" if problems else "ok", len(functions), len(type_names), len(names), len(examples), len(problems)))
sys.exit(1 if problems else 0)
