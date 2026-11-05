#!/usr/bin/env python3
"""Regenerates the Unicode width tables inside tic.h.

Run it after upgrading Python to pick up a newer Unicode version:

    python3 tools/gen_width.py          # rewrites the tables in tic.h
    python3 tools/gen_width.py --check  # exits with 1 if they are out of date

The tables come from Python's own `unicodedata` module, so there's nothing to
download. They hold code points that take up zero or two terminal cells. Every
other code point takes one.

The rules below follow what terminals do (the same logic as most wcwidth tables):

  width 0  Combining marks (Mn, Me), invisible format characters (Cf), line and
           paragraph separators (Zl, Zp), and the Hangul Jamo vowels and final
           consonants, which join onto the syllable before them.
  width 2  East Asian "Wide" and "Fullwidth" characters. Emoji that display as
           pictures by default are in this group too.
"""

import os
import re
import sys
import unicodedata

HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tic.h")
BEGIN = "/* BEGIN GENERATED WIDTH TABLES (tools/gen_width.py) */\n"
END = "/* END GENERATED WIDTH TABLES */\n"

# These are in category Cf, but they draw a visible sign, so terminals give them
# a cell. Soft hyphen (U+00AD) and the Arabic/Syriac number signs are the main ones.
VISIBLE_FORMAT = {0x00AD, 0x0600, 0x0601, 0x0602, 0x0603, 0x0604, 0x0605, 0x06DD,
                  0x070F, 0x0890, 0x0891, 0x08E2, 0x110BD, 0x110CD}


def width(cp):
    ch = chr(cp)
    category = unicodedata.category(ch)
    if 0xD800 <= cp <= 0xDFFF:
        return 1  # surrogates aren't real characters; TIC never draws them
    if category in ("Mn", "Me", "Zl", "Zp"):
        return 0
    if category == "Cf" and cp not in VISIBLE_FORMAT:
        return 0
    if 0x1160 <= cp <= 0x11FF or 0xD7B0 <= cp <= 0xD7FF:
        return 0  # Hangul Jamo vowels and final consonants
    if unicodedata.east_asian_width(ch) in ("W", "F"):
        return 2
    return 1


def ranges(wanted):
    """Compresses every code point of the given width into [first, last] ranges."""
    result = []
    start = None
    for cp in range(0x110000):
        if width(cp) == wanted:
            if start is None:
                start = cp
        elif start is not None:
            result.append((start, cp - 1))
            start = None
    if start is not None:
        result.append((start, 0x10FFFF))
    return result


def c_table(name, items):
    lines = ["static const tic__range %s[] = {" % name]
    row = []
    for first, last in items:
        row.append("{0x%X,0x%X}," % (first, last))
        if len(row) == 5:
            lines.append("    " + " ".join(row))
            row = []
    if row:
        lines.append("    " + " ".join(row))
    lines.append("};")
    return "\n".join(lines) + "\n"


def generated():
    return (
        BEGIN
        + "/* Unicode %s. Don't edit by hand: see tools/gen_width.py. */\n" % unicodedata.unidata_version
        + c_table("tic__zero_width", ranges(0))
        + c_table("tic__double_width", ranges(2))
        + END
    )


def main():
    with open(HEADER, encoding="utf-8", newline="") as f:
        text = f.read()
    pattern = re.compile(re.escape(BEGIN) + ".*?" + re.escape(END), re.S)
    if not pattern.search(text):
        sys.exit("generated-table markers not found in tic.h")
    updated = pattern.sub(lambda _: generated(), text)
    if "--check" in sys.argv:
        if updated != text:
            sys.exit("tic.h width tables are out of date: run tools/gen_width.py")
        print("width tables are up to date")
        return
    with open(HEADER, "w", encoding="utf-8", newline="") as f:
        f.write(updated)


if __name__ == "__main__":
    main()
