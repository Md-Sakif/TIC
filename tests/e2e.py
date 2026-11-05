#!/usr/bin/env python3
"""End-to-end tests: runs a real TIC program inside a real pseudo-terminal.

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
import socket
import struct
import subprocess
import sys
import tempfile
import termios
import time
import traceback

APP = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "build/e2e_app")
failures = []
current_test = "startup"

# Sanitizer builds have their own crash handlers, which report the crash and exit with
# status 1. TIC chains to them correctly, but the crash tests also check the exit
# status, so ask the sanitizer to leave those signals alone.
TEST_ENV = dict(os.environ, ASAN_OPTIONS="handle_segv=0:handle_sigbus=0:handle_sigfpe=0:handle_abort=0")


# The session leader of a terminal is the process that owns it. When that process exits,
# macOS revokes the terminal, so even a file descriptor opened earlier stops working
# and its settings can't be read. Linux keeps it readable. The tests need to read the
# settings after the app has exited, because that's how they prove they were restored.
#
# So the app is never the leader. This small program takes that role, runs the app as its
# child, reports the app's pid and exit status over a socket, and stays alive until the
# test closes that socket. A Python handler is used for SIGHUP, rather than ignoring it,
# because handlers are reset when the app starts but "ignored" would be inherited.
HOLDER = r"""
import fcntl, os, signal, socket, subprocess, sys, termios

control = socket.socket(fileno=int(sys.argv[1]))
ignore_signal = int(sys.argv[2])
fcntl.ioctl(0, termios.TIOCSCTTY, 0)
signal.signal(signal.SIGHUP, lambda *_: None)

def child_setup():
    if ignore_signal:
        signal.signal(ignore_signal, signal.SIG_IGN)

child = subprocess.Popen(sys.argv[3:], preexec_fn=child_setup)
control.sendall(b"pid %d\n" % child.pid)
control.sendall(b"exit %d\n" % child.wait())
while control.recv(1):
    pass
"""


# A miniature job-control shell, for testing Ctrl+Z. It does what a real shell does: it
# puts the app in its own process group, gives that group the terminal, and waits with
# WUNTRACED so it hears when the app stops. It then takes the terminal back, and tells
# the test. When the test says to resume, it gives the terminal back and sends SIGCONT.
#
# Being written here means it behaves the same everywhere, whatever bash is installed.
# It makes a failure easy to read: if this never reports a stop, the app didn't stop.
JOB_SHELL = r"""
import fcntl, os, signal, socket, sys, termios

control = socket.socket(fileno=int(sys.argv[1]))
args = sys.argv[3:]
fcntl.ioctl(0, termios.TIOCSCTTY, 0)
signal.signal(signal.SIGTTOU, signal.SIG_IGN)  # or taking the terminal back stops this shell
shell_group = os.getpgrp()

pid = os.fork()
if pid == 0:
    control.close()
    os.setpgid(0, 0)
    os.tcsetpgrp(0, os.getpid())
    signal.signal(signal.SIGTTOU, signal.SIG_DFL)
    os.execv(args[0], args)

try:
    os.setpgid(pid, pid)  # both sides do this, because either could run first
except OSError:
    pass
os.tcsetpgrp(0, pid)
control.sendall(b"pid %d\n" % pid)

while True:
    _, status = os.waitpid(pid, os.WUNTRACED)
    if not os.WIFSTOPPED(status):
        control.sendall(b"exit %d\n" % os.waitstatus_to_exitcode(status))
        break
    os.tcsetpgrp(0, shell_group)
    control.sendall(b"stopped %d\n" % os.WSTOPSIG(status))
    if not control.recv(1):
        break
    os.tcsetpgrp(0, pid)
    os.kill(-pid, signal.SIGCONT)

while control.recv(1):
    pass
"""


class App:
    """The running app. It has the parts of subprocess.Popen the tests use."""

    def __init__(self, args, terminal_fd, ignore_signal, parent_script=HOLDER):
        self.control, holder_end = socket.socketpair()
        self.stop_signal = None
        self.holder = subprocess.Popen(
            [sys.executable, "-c", parent_script, str(holder_end.fileno()), str(ignore_signal or 0)] + args,
            stdin=terminal_fd,
            stdout=terminal_fd,
            stderr=terminal_fd,
            start_new_session=True,
            pass_fds=(holder_end.fileno(),),
            env=TEST_ENV,
        )
        holder_end.close()
        self.received = b""
        self.pid = None
        self.code = None
        self._read_messages(10)
        if self.pid is None:
            raise RuntimeError("the holder process did not start the app")

    def _read_messages(self, seconds):
        ready, _, _ = select.select([self.control], [], [], seconds)
        if not ready:
            return
        self.received += self.control.recv(4096)
        while b"\n" in self.received:
            line, self.received = self.received.split(b"\n", 1)
            kind, value = line.split()
            if kind == b"pid":
                self.pid = int(value)
            elif kind == b"exit":
                self.code = int(value)
            elif kind == b"stopped":
                self.stop_signal = int(value)

    def resume(self):
        """Tells the job-control shell to continue the stopped app."""
        self.stop_signal = None
        self.control.sendall(b"c")

    def poll(self):
        if self.code is None:
            self._read_messages(0)
        return self.code

    def wait(self, timeout):
        deadline = time.time() + timeout
        while self.poll() is None:
            if time.time() >= deadline:
                raise TimeoutError("the app is still running")
            self._read_messages(0.01)
        return self.code

    def send_signal(self, number):
        if self.poll() is None:
            os.kill(self.pid, number)

    def stop(self):
        """Ends the app if needed, then lets the holder exit too."""
        self.send_signal(signal.SIGKILL)
        self.control.close()
        self.holder.wait(timeout=5)


class Terminal:
    """A pseudo-terminal with one program running inside it."""

    def __init__(self, cols=80, rows=24, timeout=None, ignore_signal=None, parent_script=HOLDER):
        self.master, self.slave = pty.openpty()
        self.resize(cols, rows)
        self.settings_before = termios.tcgetattr(self.slave)
        self.dir = tempfile.TemporaryDirectory()
        self.log_path = os.path.join(self.dir.name, "events.log")
        self.output = b""
        args = [APP, self.log_path]
        if timeout is not None:
            args.append(str(timeout))

        self.process = App(args, self.slave, ignore_signal, parent_script)

    def resize(self, cols, rows):
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

    def wait_for_log(self, line, seconds=5):
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
        return "timed out"

    def wait_for_stop(self, seconds=5):
        deadline = time.time() + seconds
        while time.time() < deadline:
            self.drain(0.01)
            self.process.poll()
            if self.process.stop_signal is not None:
                return True
        return False

    def settings_changes(self):
        """Returns "" if the terminal settings are back to how they started."""
        return settings_difference(self.settings_before, termios.tcgetattr(self.slave))

    def close(self):
        self.process.stop()
        os.close(self.master)
        os.close(self.slave)
        self.dir.cleanup()


# BSD kernels, which macOS's is based on, can set PENDIN by themselves when a terminal
# switches to line mode. It means "input is waiting to be reprocessed". It's the kernel's
# own status rather than a setting, so a restored terminal can have it. Only a difference
# of exactly this bit is forgiven, and a note is printed whenever that happens.
PENDIN = getattr(termios, "PENDIN", 0)


def settings_difference(before, after):
    """Returns "" if two sets of terminal settings match, or a description if they don't."""
    names = ["iflag", "oflag", "cflag", "lflag", "ispeed", "ospeed", "cc"]
    differences = []
    for name, old, new in zip(names, before, after):
        if name == "lflag" and (old ^ new) == PENDIN and PENDIN:
            print("    note: the kernel set PENDIN (expected on BSD and macOS)")
            continue
        if old != new:
            if name == "cc":
                changed = [i for i, (a, b) in enumerate(zip(old, new)) if a != b]
                differences.append("cc entries %s" % changed)
            elif isinstance(old, int) and name.endswith("flag"):
                differences.append("%s was 0x%x, now 0x%x (changed bits 0x%x)" % (name, old, new, old ^ new))
            else:
                differences.append("%s was %s, now %s" % (name, old, new))
    return "; ".join(differences)


def report_failure(message):
    failures.append(message)
    print("    FAIL:", message)
    # On GitHub, this becomes an annotation. Job logs are only visible to repo admins,
    # but anyone can read annotations, and they appear on the run's summary page.
    if os.environ.get("GITHUB_ACTIONS"):
        text = "%s: %s" % (current_test, message)
        print("::error::" + text.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A"))


def check(condition, message):
    if not condition:
        report_failure(message)


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
    difference = t.settings_changes()
    check(not difference, "terminal settings were not restored: " + difference)
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


def check_crash_recovery(trigger, expected_exit):
    t = Terminal()
    t.wait_for_log("ready 80 24")
    check(is_raw(t), "terminal should be raw before the crash")
    t.drain()
    t.output = b""
    trigger(t)
    code = t.wait_exit()
    output = t.drain()
    check(code == expected_exit, "exit status was %s, expected %s" % (code, expected_exit))
    difference = t.settings_changes()
    check(not difference, "terminal settings were not restored (exit %s): %s" % (code, difference))
    check(b"\x1b[?1049l" in output, "did not leave the alternate screen (exit %s)" % code)
    check(b"\x1b[?25h" in output, "did not show the cursor again (exit %s)" % code)
    t.close()


def test_recovers_from_exit_call():
    # The app calls exit() without ever calling tic_shutdown().
    check_crash_recovery(lambda t: t.send(b"\x05"), 7)


def test_recovers_from_abort():
    check_crash_recovery(lambda t: t.send(b"\x07"), -signal.SIGABRT)


def test_recovers_from_signals():
    for number in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP, signal.SIGQUIT, signal.SIGSEGV, signal.SIGBUS, signal.SIGFPE):
        check_crash_recovery(lambda t: t.process.send_signal(number), -number)


def test_ignored_signal_stays_ignored():
    # `nohup` makes SIGHUP ignored, and TIC has to respect that.
    t = Terminal(ignore_signal=signal.SIGHUP)
    t.wait_for_log("ready 80 24")
    t.process.send_signal(signal.SIGHUP)
    time.sleep(0.1)
    check(t.process.poll() is None, "an ignored SIGHUP killed the app")
    t.send(b"\x11")
    check(t.wait_exit() == 0, "app did not exit cleanly")
    t.close()


def test_suspend_with_no_shell_keeps_working():
    # With no shell above it the OS discards the stop, so the app has to carry on.
    t = Terminal()
    t.wait_for_log("ready 80 24")
    t.send(b"\x13")
    check(t.wait_for_log("suspend 0"), "tic_suspend did not return")
    check(is_raw(t), "terminal should be raw again")
    t.send(b"a")
    check(t.wait_for_log("key 97 0"), "input stopped working after the suspend")
    t.send(b"\x11")
    check(t.wait_exit() == 0, "app did not exit cleanly")
    difference = t.settings_changes()
    check(not difference, "terminal settings were not restored: " + difference)
    t.close()


def test_suspend_under_a_job_control_shell():
    t = Terminal(parent_script=JOB_SHELL)
    check(t.wait_for_log("ready 80 24"), "app did not start")
    check(is_raw(t), "terminal should be raw while running")
    t.drain()
    t.output = b""

    t.send(b"\x13")
    if not t.wait_for_stop():
        check(False, "the app never stopped. Log: %s" % t.log())
        t.close()
        return
    check(t.process.stop_signal == signal.SIGTSTP, "stopped by signal %s" % t.process.stop_signal)
    output = t.drain(0.1)
    check(b"\x1b[?1049l" in output, "did not leave the alternate screen before stopping")
    difference = t.settings_changes()
    check(not difference, "the shell got a terminal that wasn't restored: " + difference)

    t.output = b""
    t.process.resume()
    check(t.wait_for_log("suspend 0"), "tic_suspend did not return after resuming")
    check(t.wait_for_log("resize 80 24"), "resuming should ask the app to redraw")
    check(is_raw(t), "terminal should be raw again after resuming")
    check(b"\x1b[?1049h" in t.drain(0.1), "did not go back to the alternate screen")

    t.send(b"\x11")
    check(t.wait_exit() == 0, "app did not exit cleanly")
    difference = t.settings_changes()
    check(not difference, "terminal settings were not restored: " + difference)
    t.close()


def test_stop_and_continue_signals():
    # SIGSTOP can't be caught. The app only finds out when SIGCONT arrives.
    t = Terminal()
    t.wait_for_log("ready 80 24")
    t.send(b"\x10")
    t.wait_for_log("key 112 4")
    t.drain(0.1)
    t.process.send_signal(signal.SIGSTOP)
    time.sleep(0.1)
    t.output = b""
    t.process.send_signal(signal.SIGCONT)
    check(t.wait_for_log("resize 80 24"), "resuming should ask the app to redraw")
    t.send(b"\x10")
    t.wait_for_log("key 112 4")
    output = t.drain(0.2)
    check(b"\x1b[?1049h" in output, "did not go back to the alternate screen")
    check(b"\x1b[?1002h" in output, "did not turn mouse reporting back on")
    check(b"frame-marker" in output, "did not repaint after resuming")
    check(t.process.poll() is None, "the app should still be running")
    t.send(b"\x11")
    check(t.wait_exit() == 0, "app did not exit cleanly")
    t.close()


def test_suspend_under_bash():
    # The same flow as the job-control test, with a real bash. It's skipped on old versions
    # (macOS ships 3.2), because a stopped app hung bash on the macOS CI machine, and
    # job control from `set -m` in a script changed around 4.4. That cause is a suspicion
    # rather than a proven fact: test_suspend_under_a_job_control_shell is the real check.
    version = subprocess.check_output(["bash", "-c", "echo ${BASH_VERSINFO[0]} ${BASH_VERSINFO[1]}"]).split()
    print("    bash %s.%s" % (version[0].decode(), version[1].decode()))
    if (int(version[0]), int(version[1])) < (4, 4):
        print("    skipped: this bash is too old")
        return

    master, slave = pty.openpty()
    fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
    before = termios.tcgetattr(slave)

    def become_terminal_owner():
        fcntl.ioctl(0, termios.TIOCSCTTY, 0)

    with tempfile.TemporaryDirectory() as folder:
        log = os.path.join(folder, "log")
        # The sleep at the end keeps the shell, the session leader here, alive for the final
        # check. See the comment above HOLDER.
        script = 'set -m; "$1" "$2"; echo AFTER-STOP; sleep 0.3; fg; echo AFTER-FG; sleep 1'
        shell = subprocess.Popen(
            ["bash", "-c", script, "bash", APP, log],
            stdin=slave, stdout=slave, stderr=slave,
            start_new_session=True, preexec_fn=become_terminal_owner,
        )
        output = b""

        def pump(until_text=None, until_log=None, seconds=4):
            nonlocal output
            deadline = time.time() + seconds
            while time.time() < deadline:
                ready, _, _ = select.select([master], [], [], 0.01)
                if ready:
                    try:
                        output += os.read(master, 65536)
                    except OSError:
                        return False
                if until_text and until_text in output:
                    return True
                if until_log and os.path.exists(log):
                    with open(log, encoding="utf-8", errors="replace") as f:
                        if until_log in f.read().splitlines():
                            return True
            return False

        check(pump(until_log="ready 80 24"), "app did not start under the shell")
        check(not termios.tcgetattr(slave)[3] & termios.ECHO, "terminal should be raw while running")

        os.write(master, b"\x13")
        check(pump(until_text=b"AFTER-STOP"), "the app did not stop")
        check(b"\x1b[?1049l" in output, "did not leave the alternate screen before stopping")
        attrs = termios.tcgetattr(slave)
        check(attrs[3] & termios.ECHO and attrs[3] & termios.ICANON, "the shell got a terminal still in raw mode")

        check(pump(until_log="suspend 0"), "the app did not resume after fg")
        check(pump(until_log="resize 80 24"), "resuming should ask the app to redraw")
        check(not termios.tcgetattr(slave)[3] & termios.ECHO, "terminal should be raw again after fg")

        os.write(master, b"\x11")
        check(pump(until_text=b"AFTER-FG"), "the app did not finish")
        difference = settings_difference(before, termios.tcgetattr(slave))
        check(not difference, "terminal settings were not restored at the end: " + difference)
        try:
            code = shell.wait(timeout=5)
            check(code == 0, "shell exit status was %s" % code)
        except subprocess.TimeoutExpired:
            check(False, "the shell never finished")
            shell.kill()
            shell.wait()
    os.close(master)
    os.close(slave)


TESTS = [value for name, value in sorted(globals().items()) if name.startswith("test_")]


def main():
    global current_test
    for test in TESTS:
        current_test = test.__name__
        print("  " + test.__name__, flush=True)
        # One test blowing up mustn't hide the results of the others.
        try:
            test()
        except Exception:
            report_failure("crashed with:\n" + traceback.format_exc())
    print("%s: %d e2e tests, %d failed" % ("FAILED" if failures else "ok", len(TESTS), len(failures)))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
