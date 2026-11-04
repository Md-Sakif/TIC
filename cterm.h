/* cterm.h - a cross-platform, single-header terminal UI library for C and C++.
 *
 * In exactly one source file, before any other #include:
 *
 *     #define CTERM_IMPLEMENTATION
 *     #include "cterm.h"
 *
 * Every other file just does #include "cterm.h".
 *
 * See README.md for the guide. License: MIT, see the end of this file.
 */

/* ------------------------------------------------------------------------- *
 * Feature-test macros
 *
 * These have to come before the very first system header in the file, because
 * libc reads them once and then never again. That is why they sit above the
 * public section (which includes <stdint.h>), and why the documentation says
 * CTERM_IMPLEMENTATION must come first: with a strict `-std=c99`, a system
 * header included earlier would lock libc into hiding sigaction() and friends.
 * ------------------------------------------------------------------------- */
#if defined(CTERM_IMPLEMENTATION) && !defined(CTERM_IMPLEMENTATION_DONE) && !defined(_WIN32)
  #ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
  #endif
  /* Asking for POSIX hides the non-POSIX parts we also need (SIGWINCH and
   * TIOCGWINSZ) on some systems, so ask each libc to show them again. */
  #ifndef _DEFAULT_SOURCE
    #define _DEFAULT_SOURCE    /* glibc */
  #endif
  #ifndef _DARWIN_C_SOURCE
    #define _DARWIN_C_SOURCE   /* macOS */
  #endif
  #ifndef _NETBSD_SOURCE
    #define _NETBSD_SOURCE     /* NetBSD */
  #endif
  #ifndef _BSD_SOURCE
    #define _BSD_SOURCE        /* OpenBSD */
  #endif
  #if (defined(__FreeBSD__) || defined(__DragonFly__)) && !defined(__BSD_VISIBLE)
    #define __BSD_VISIBLE 1    /* FreeBSD, DragonFly */
  #endif
#endif

/* ========================================================================= *
 * PUBLIC API
 * ========================================================================= */
#ifndef CTERM_H
#define CTERM_H

/* CTERM_API goes in front of every public function.
 *
 *   default          `extern`: one file holds the code, the rest link to it.
 *   CTERM_STATIC     `static`: each file gets a private copy. Only safe when
 *                    all cterm calls are in ONE source file, because the
 *                    terminal is shared by the whole process.
 *   your own value   e.g. __declspec(dllexport) when building a Windows DLL. */
#ifndef CTERM_API
  #ifdef CTERM_STATIC
    #if defined(__GNUC__) || defined(__clang__)
      /* `unused` silences warnings for the functions you don't happen to call. */
      #define CTERM_API static __attribute__((unused))
    #else
      #define CTERM_API static
    #endif
  #else
    #define CTERM_API extern
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* --- Lifecycle ----------------------------------------------------------- */

/* Takes over the terminal. Returns 0 on success, or -1 on failure with the
 * reason in cterm_error(). Fails if stdin or stdout isn't a terminal. */
CTERM_API int cterm_init(void);

/* Gives the terminal back. Safe to call more than once. Also runs automatically
 * at exit() and on fatal signals, so a crash doesn't leave a broken shell. */
CTERM_API void cterm_shutdown(void);

/* A description of the last failure. Never NULL; "" if nothing has failed. */
CTERM_API const char *cterm_error(void);

/* Reads raw input bytes without waiting. Returns how many were read (0 if none
 * are available) or -1 if the terminal has gone away. */
CTERM_API int cterm_read(unsigned char *buf, int cap);

#ifdef __cplusplus
}
#endif
#endif /* CTERM_H */


/* ========================================================================= *
 * IMPLEMENTATION
 *
 * This block is deliberately outside the CTERM_H guard, so that
 *     #include "cterm.h"
 *     #define CTERM_IMPLEMENTATION
 *     #include "cterm.h"
 * still produces the code. CTERM_IMPLEMENTATION_DONE stops it appearing twice.
 *
 * Internal names start with `cterm__` (two underscores). They are static, so
 * they can't collide with anything outside this file.
 * ========================================================================= */
#ifdef CTERM_IMPLEMENTATION
#ifndef CTERM_IMPLEMENTATION_DONE
#define CTERM_IMPLEMENTATION_DONE

#include <stddef.h>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN   /* skip rarely used parts of windows.h */
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX              /* stop windows.h defining min() and max() */
  #endif
  #include <windows.h>
#else
  #include <errno.h>
  #include <termios.h>
  #include <unistd.h>
#endif

/* --- Errors -------------------------------------------------------------- */

static const char *cterm__error_text = "";

/* `text` must be a string literal: it's stored, not copied. */
static int cterm__fail(const char *text) {
    cterm__error_text = text;
    return -1;
}

CTERM_API const char *cterm_error(void) {
    return cterm__error_text;
}

/* ========================================================================= *
 * PLATFORM LAYER
 *
 * Everything that differs between operating systems lives here, behind these
 * private functions. Porting cterm to a new OS means writing this set again:
 *
 *   cterm__plat_init       enter raw mode; 0 on success, -1 via cterm__fail()
 *   cterm__plat_shutdown   undo cterm__plat_init
 *   cterm__plat_read       read available input as UTF-8/VT bytes without
 *                          waiting; >0 bytes, 0 none, -1 terminal gone
 * ========================================================================= */
#if defined(_WIN32)

/* ------------------------------ Windows ---------------------------------- */

static HANDLE cterm__hin, cterm__hout;
static DWORD  cterm__saved_in_mode, cterm__saved_out_mode;
static UINT   cterm__saved_in_cp, cterm__saved_out_cp;

/* True if the console accepted ENABLE_VIRTUAL_TERMINAL_INPUT, meaning special
 * keys arrive as the same escape sequences a Unix terminal would send. */
static int cterm__vt_input;

static int cterm__plat_init(void) {
    DWORD in_mode, out_mode;

    cterm__hin  = GetStdHandle(STD_INPUT_HANDLE);
    cterm__hout = GetStdHandle(STD_OUTPUT_HANDLE);

    /* GetConsoleMode only succeeds on a real console, not on a pipe or file. */
    if (!GetConsoleMode(cterm__hin, &cterm__saved_in_mode) ||
        !GetConsoleMode(cterm__hout, &cterm__saved_out_mode)) {
        return cterm__fail("stdin and stdout must both be a console");
    }
    cterm__saved_in_cp  = GetConsoleCP();
    cterm__saved_out_cp = GetConsoleOutputCP();

    /* Output: interpret escape sequences, like every other terminal does. */
    out_mode = cterm__saved_out_mode | ENABLE_PROCESSED_OUTPUT |
               ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    if (!SetConsoleMode(cterm__hout, out_mode)) {
        /* Some consoles reject just the newline flag, so retry without it. */
        out_mode &= ~(DWORD)DISABLE_NEWLINE_AUTO_RETURN;
        if (!SetConsoleMode(cterm__hout, out_mode)) {
            return cterm__fail("this console has no escape sequence support (Windows 10 or newer is required)");
        }
    }

    /* Input is Windows' version of raw mode: no echo, no waiting for Enter, and
     * Ctrl+C arrives as a key. QuickEdit is left out because it would freeze
     * the program whenever the user clicks in the window. */
    in_mode = ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;
    cterm__vt_input = SetConsoleMode(cterm__hin, in_mode | ENABLE_VIRTUAL_TERMINAL_INPUT) != 0;
    if (!cterm__vt_input && !SetConsoleMode(cterm__hin, in_mode)) {
        SetConsoleMode(cterm__hout, cterm__saved_out_mode);
        return cterm__fail("could not configure console input");
    }

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    return 0;
}

static void cterm__plat_shutdown(void) {
    SetConsoleMode(cterm__hin, cterm__saved_in_mode);
    SetConsoleMode(cterm__hout, cterm__saved_out_mode);
    SetConsoleCP(cterm__saved_in_cp);
    SetConsoleOutputCP(cterm__saved_out_cp);
}

/* Appends one Unicode code point as UTF-8. The caller checks there's room. */
static int cterm__win_put_utf8(unsigned char *out, unsigned long cp) {
    if (cp < 0x80) {
        out[0] = (unsigned char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (unsigned char)(0xC0 | (cp >> 6));
        out[1] = (unsigned char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (unsigned char)(0xE0 | (cp >> 12));
        out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (unsigned char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (unsigned char)(0xF0 | (cp >> 18));
    out[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (unsigned char)(0x80 | (cp & 0x3F));
    return 4;
}

/* Windows reports text as UTF-16, with characters above U+FFFF split into two
 * "surrogate" units that may arrive in separate records. The first half waits
 * here until the second shows up. */
static WCHAR cterm__high_surrogate;

static int cterm__win_put_utf16(unsigned char *out, WCHAR unit) {
    unsigned long cp = unit;
    if (unit >= 0xD800 && unit <= 0xDBFF) {
        cterm__high_surrogate = unit;
        return 0;
    }
    if (unit >= 0xDC00 && unit <= 0xDFFF) {
        if (!cterm__high_surrogate) return 0; /* stray half: drop it */
        cp = 0x10000 + (((unsigned long)cterm__high_surrogate - 0xD800) << 10) + (unit - 0xDC00);
    }
    cterm__high_surrogate = 0;
    return cterm__win_put_utf8(out, cp);
}

static int cterm__plat_read(unsigned char *buf, int cap) {
    INPUT_RECORD records[64];
    DWORD available = 0, count = 0, i;
    int len = 0;

    if (!GetNumberOfConsoleInputEvents(cterm__hin, &available)) return -1;
    if (available == 0) return 0;
    if (!ReadConsoleInputW(cterm__hin, records, 64, &count)) return -1;

    for (i = 0; i < count; i++) {
        const KEY_EVENT_RECORD *key;
        int repeat;
        if (records[i].EventType != KEY_EVENT) continue;
        key = &records[i].Event.KeyEvent;
        if (!key->bKeyDown || key->uChar.UnicodeChar == 0) continue;
        for (repeat = key->wRepeatCount; repeat > 0 && len + 4 <= cap; repeat--) {
            len += cterm__win_put_utf16(buf + len, key->uChar.UnicodeChar);
        }
    }
    return len;
}

#else

/* ------------------------------- POSIX ----------------------------------- */

static struct termios cterm__saved_termios;

static int cterm__plat_init(void) {
    struct termios raw;

    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        return cterm__fail("stdin and stdout must both be a terminal");
    }
    if (tcgetattr(STDIN_FILENO, &cterm__saved_termios) != 0) {
        return cterm__fail("could not read the terminal settings");
    }

    /* Raw mode: the kernel stops helping and passes every byte straight
     * through. This is what cfmakeraw() does, but that function isn't POSIX,
     * so it's hidden under -std=c99. */
    raw = cterm__saved_termios;

    /* No Ctrl+S/Ctrl+Q flow control, no Enter translation, and no stripping of
     * the top bit (ISTRIP would corrupt UTF-8). */
    raw.c_iflag &= ~(tcflag_t)(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);

    /* No output processing: "\n" means exactly "\n", not "\r\n". */
    raw.c_oflag &= ~(tcflag_t)OPOST;

    /* No echo, no line buffering, no Ctrl+C/Ctrl+Z signals, no Ctrl+V handling.
     * The app sees those keys as ordinary input. */
    raw.c_lflag &= ~(tcflag_t)(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

    /* 8-bit characters, no parity. */
    raw.c_cflag &= ~(tcflag_t)(CSIZE | PARENB);
    raw.c_cflag |= CS8;

    /* VMIN=0, VTIME=0: read() returns at once, even when there's no input. */
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;

    /* TCSAFLUSH finishes pending output and discards keys typed before now, so
     * they don't get mistaken for input to the app. */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return cterm__fail("could not switch the terminal to raw mode");
    }
    return 0;
}

static void cterm__plat_shutdown(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &cterm__saved_termios);
}

static int cterm__plat_read(unsigned char *buf, int cap) {
    ssize_t n = read(STDIN_FILENO, buf, (size_t)cap);
    if (n < 0) {
        return (errno == EAGAIN || errno == EINTR) ? 0 : -1;
    }
    return (int)n;
}

#endif /* platform */

/* ========================================================================= *
 * LIFECYCLE
 * ========================================================================= */

static int cterm__active = 0;

CTERM_API int cterm_init(void) {
    /* Calling init twice must not save raw settings as the "normal" ones. */
    if (cterm__active) return 0;
    if (cterm__plat_init() != 0) return -1;
    cterm__active = 1;
    return 0;
}

CTERM_API void cterm_shutdown(void) {
    if (!cterm__active) return;
    cterm__plat_shutdown();
    cterm__active = 0;
}

CTERM_API int cterm_read(unsigned char *buf, int cap) {
    if (!cterm__active) return cterm__fail("cterm_init has not been called");
    return cterm__plat_read(buf, cap);
}

#endif /* CTERM_IMPLEMENTATION_DONE */
#endif /* CTERM_IMPLEMENTATION */
