#!/usr/bin/env python3
"""End-to-end tests: runs a real cterm program inside a real pseudo-terminal.

The unit tests prove the parser and renderer are correct on their own. These prove
everything works together with an actual OS terminal: raw mode, restoring settings,
resize signals, timing, and crash recovery.

POSIX only, because it uses Python's pty module. Run it with `make e2e`.
"""

import fcntl
import os
import pty
import select
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time

APP = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "build/e2e_app")
failures = []


class Terminal:
    """A pseudo-terminal with one program running inside it."""

    def __init__(self, cols=80, rows=24, timeout=None):
        self.master, self.slave = pty.openpty()
        self.resize(cols, rows, signal_app=False)
        self.settings_before = termios.tcgetattr(self.slave)
        self.dir = tempfile.TemporaryDirectory()
        self.log_path = os.path.join(self.dir.name, "events.log")
        self.output = b""
        args = [APP, self.log_path]
        if timeout is not None:
            args.append(str(timeout))

        def become_terminal_owner():
            fcntl.ioctl(0, termios.TIOCSCTTY, 0)

        self.process = subprocess.Popen(
            args,
            stdin=self.slave,
            stdout=self.slave,
            stderr=self.slave,
            start_new_session=True,
            preexec_fn=become_terminal_owner,
        )

    def resize(self, cols, rows, signal_app=True):
        # Setting the size on a terminal makes the kernel send SIGWINCH to its program.
        fcntl.ioctl(self.master, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))

    def send(self, data):
        os.write(self.master, data)

    def drain(self, seconds=0.05):
        """Collects whatever the program has written to the screen."""
        deadline = time.time() + seconds
        while time.time() < deadline:
            ready, _, _ = select.select([self.master], [], [], 0.01)
            if ready:
                try:
                    self.output += os.read(self.master, 65536)
                except OSError:
                    break
        return self.output

    def log(self):
        try:
            with open(self.log_path, encoding="utf-8", errors="replace") as f:
                return f.read().splitlines()
        except FileNotFoundError:
            return []

    def wait_for_log(self, line, seconds=3):
        """Waits until the log contains a line, then returns True."""
        deadline = time.time() + seconds
        while time.time() < deadline:
            self.drain(0.01)
            if line in self.log():
                return True
        return False

    def wait_exit(self, seconds=3):
        deadline = time.time() + seconds
        while time.time() < deadline:
            self.drain(0.01)
            code = self.process.poll()
            if code is not None:
                return code
        self.process.kill()
        self.process.wait()
        return "timed out"

    def restored(self):
        return termios.tcgetattr(self.slave) == self.settings_before

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait()
        os.close(self.master)
        os.close(self.slave)
        self.dir.cleanup()


def check(condition, message):
    if not condition:
        failures.append(message)
        print("    FAIL:", message)


def is_raw(term):
    attrs = termios.tcgetattr(term.slave)
    return not attrs[3] & (termios.ECHO | termios.ICANON | termios.ISIG)


def test_startup_and_shutdown():
    t = Terminal(cols=100, rows=30)
    check(t.wait_for_log("ready 100 30"), "app did not report the terminal size")
    check(is_raw(t), "terminal is not in raw mode")
    output = t.drain()
    check(b"\x1b[?1049h" in output, "did not enter the alternate screen")
    check(b"\x1b[?2004h" in output, "did not enable bracketed paste")
    t.send(b"\x11")
    check(t.wait_exit() == 0, "app did not exit cleanly")
    output = t.drain()
    check(t.restored(), "terminal settings were not restored")
    check(b"\x1b[?1049l" in output, "did not leave the alternate screen")
    check(b"\x1b[?25h" in output, "did not show the cursor again")
    check(b"\x1b[?1006l" in output, "did not turn off mouse reporting")
    t.close()


def test_not_a_terminal():
    with tempfile.TemporaryDirectory() as folder:
        log = os.path.join(folder, "log")
        with open(os.devnull) as null:
            code = subprocess.call([APP, log], stdin=null, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        check(code == 3, "init should fail when stdin is not a terminal, exit was %s" % code)
        with open(log, encoding="utf-8") as f:
            text = f.read()
        check("must both be a terminal" in text, "wrong error message: %r" % text)


def test_keys_arrive_as_events():
    t = Terminal()
    t.wait_for_log("ready 80 24")
    t.send(b"a")
    check(t.wait_for_log("key 97 0"), "plain key was not reported")
    t.send(b"\x1b[A")
    up = 0x110004
    check(t.wait_for_log("key %d 0" % up), "Up arrow was not reported")
    t.send("ক".encode())
    check(t.wait_for_log("key %d 0" % 0x995), "Bengali character was not reported")
    t.send(b"\x03")
    check(t.wait_for_log("key 99 4"), "Ctrl+C should be a key, not a signal")
    check(t.process.poll() is None, "Ctrl+C must not kill the app")
    t.send(b"\x11")
    t.wait_exit()
    t.close()


def test_esc_key_timing():
    t = Terminal()
    t.wait_for_log("ready 80 24")
    esc = 0x110003
    start = time.time()
    t.send(b"\x1b")
    check(t.wait_for_log("key %d 0" % esc), "Esc was not reported")
    elapsed = time.time() - start
    check(elapsed < 0.5, "Esc took too long: %.3fs" % elapsed)
    t.send(b"\x11")
    t.wait_exit()
    t.close()


def test_esc_with_zero_timeout():
    # An app that polls with timeout 0 still has to see Esc once time has passed.
    t = Terminal(timeout=0)
    t.wait_for_log("ready 80 24")
    esc = 0x110003
    t.send(b"\x1b")
    check(t.wait_for_log("key %d 0" % esc), "Esc was lost with a zero timeout")
    t.send(b"\x11")
    t.wait_exit()
    t.close()


def test_poll_timeout():
    t = Terminal(timeout=20)
    check(t.wait_for_log("timeout"), "poll did not time out")
    t.send(b"\x11")
    t.wait_exit()
    t.close()


def test_resize():
    t = Terminal(cols=80, rows=24)
    t.wait_for_log("ready 80 24")
    t.resize(120, 40)
    check(t.wait_for_log("resize 120 40"), "resize was not reported")
    t.send(b"\x10")
    t.wait_for_log("key 112 4")
    check(b"frame-marker" in t.drain(0.1), "nothing drawn after the resize")
    t.send(b"\x11")
    t.wait_exit()
    t.close()


def test_mouse_and_paste():
    t = Terminal()
    t.wait_for_log("ready 80 24")
    check(b"\x1b[?1002h" in t.drain(), "did not enable mouse reporting")
    t.send(b"\x1b[<0;5;6M")
    check(t.wait_for_log("mouse 1 1 4 5"), "mouse press was not reported")
    t.send("\x1b[200~pasted ক text\x1b[201~".encode())
    check(t.wait_for_log("paste pasted ক text"), "paste was not reported")
    t.send(b"\x11")
    t.wait_exit()
    t.close()


def test_stalled_paste_is_delivered():
    t = Terminal()
    t.wait_for_log("ready 80 24")
    t.send(b"\x1b[200~stuck")
    check(t.wait_for_log("paste stuck", seconds=2), "a paste with no end marker was never delivered")
    t.send(b"\x11")
    t.wait_exit()
    t.close()


def test_rendering_reaches_the_terminal():
    t = Terminal()
    t.wait_for_log("ready 80 24")
    t.send(b"\x10")
    t.wait_for_log("key 112 4")
    first = t.drain(0.1)
    check(b"frame-marker" in first, "text never reached the terminal")
    check(b"\x1b[0;1;31m" in first, "style never reached the terminal")
    # An identical frame must not write anything new.
    before = len(t.output)
    t.send(b"\x10")
    t.wait_for_log("key 112 4")
    t.drain(0.1)
    check(len(t.output) == before, "an identical frame wrote %d bytes" % (len(t.output) - before))
    t.send(b"\x11")
    t.wait_exit()
    t.close()


def test_hangup_ends_the_loop():
    t = Terminal()
    t.wait_for_log("ready 80 24")
    os.close(t.master)
    code = t.process.wait(timeout=3)
    # SIGHUP or an input error are both fine. What matters is it doesn't spin forever.
    check(code is not None, "app kept running after the terminal closed")
    t.master = os.open(os.devnull, os.O_RDONLY)  # so close() has something to close
    t.close()


TESTS = [value for name, value in sorted(globals().items()) if name.startswith("test_")]


def main():
    for test in TESTS:
        print("  " + test.__name__)
        test()
    print("%s: %d e2e tests, %d failed" % ("FAILED" if failures else "ok", len(TESTS), len(failures)))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
