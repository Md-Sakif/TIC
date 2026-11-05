#!/usr/bin/env python3
"""Runs every step of the system monitor tutorial in a real pseudo-terminal.

A beginner following the tutorial will run each of these programs, so each one has to
start, show what the tutorial says it shows, quit on the key it says, and leave the
terminal as it found it.

Usage: python3 tests/e2e_tutorial.py build/tutorial
"""

import os
import sys

folder = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "build/tutorial")
sys.argv = sys.argv[:1]  # e2e.py reads its own arguments when imported
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import e2e  # noqa: E402

QUIT = b"q"
CTRL_Q = b"\x11"

# The step, text it must show, and the key that quits it.
STEPS = [
    (1, b"monitor", QUIT),  # TIC skips blank cells, so the words arrive separately
    (2, b"System Monitor", QUIT),
    (3, b"Processes", QUIT),
    (4, b"Usage:", QUIT),
    (5, b"\xe2\x96\x88", QUIT),  # a solid block from a usage bar, if anything is in use
    (6, b"CPU", QUIT),
    (7, b"MEMORY", QUIT),
    (8, b"Search:", CTRL_Q),
    (9, b"Search:", CTRL_Q),
]


def wait_for_text(terminal, text, seconds=5):
    deadline = e2e.time.time() + seconds
    while e2e.time.time() < deadline:
        if text in terminal.drain(0.02):
            return True
    return False


def run_step(number, text, quit_key):
    e2e.current_test = "tutorial step %d" % number
    print("  step %d" % number, flush=True)
    e2e.APP = os.path.join(folder, "step%d" % number)
    terminal = e2e.Terminal()
    e2e.check(wait_for_text(terminal, text), "never showed %r" % text)

    if number >= 7:
        # The list must show rows. It's sorted by memory, so this little program is
        # probably off the bottom of the screen, but something will be showing a size.
        e2e.check(wait_for_text(terminal, b" MB  ") or b" GB  " in terminal.output, "the process list is empty")
    if number == 9:
        # Search for this program by name, then move to the list and back.
        terminal.output = b""
        terminal.send(b"step9")
        e2e.check(wait_for_text(terminal, b"step9"), "the search didn't find this program")
        terminal.send(b"\t\x1b[B\t")
        terminal.drain(0.2)

    terminal.send(quit_key)
    code = terminal.wait_exit()
    e2e.check(code == 0, "exit status was %s" % code)
    difference = terminal.settings_changes()
    e2e.check(not difference, "terminal settings were not restored: " + difference)
    terminal.close()


def main():
    for number, text, quit_key in STEPS:
        try:
            run_step(number, text, quit_key)
        except Exception as error:
            e2e.report_failure("crashed with: %r" % error)
    print("%s: %d tutorial steps, %d failures" % ("FAILED" if e2e.failures else "ok", len(STEPS), len(e2e.failures)))
    sys.exit(1 if e2e.failures else 0)


if __name__ == "__main__":
    main()
