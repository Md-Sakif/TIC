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

#include <stddef.h>
#include <stdint.h>

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

/* --- Unicode ------------------------------------------------------------- */

/* How many terminal cells a character takes up: 0 for combining marks, 2 for
 * wide characters such as CJK and most emoji, 1 for everything else. */
CTERM_API int cterm_char_width(uint32_t codepoint);

/* How many cells a UTF-8 string takes up. Invalid bytes count as one cell each,
 * because that's how they're drawn. */
CTERM_API int cterm_text_width(const char *utf8);

/* --- Colours and styles -------------------------------------------------- */

/* A colour is a plain number, so it can be stored and compared freely. Use one
 * of the names below, CTERM_COLOR256(n) for the 256-colour palette, or
 * CTERM_RGB(r, g, b) for true colour. */
typedef uint32_t cterm_color;

#define CTERM_DEFAULT       ((cterm_color)0) /* the terminal's own colour */
#define CTERM_COLOR256(n)   ((cterm_color)(0x01000000u | ((uint32_t)(n) & 0xFFu)))
#define CTERM_RGB(r, g, b)  ((cterm_color)(0x02000000u | (((uint32_t)(r) & 0xFFu) << 16) | \
                             (((uint32_t)(g) & 0xFFu) << 8) | ((uint32_t)(b) & 0xFFu)))

#define CTERM_BLACK          CTERM_COLOR256(0)
#define CTERM_RED            CTERM_COLOR256(1)
#define CTERM_GREEN          CTERM_COLOR256(2)
#define CTERM_YELLOW         CTERM_COLOR256(3)
#define CTERM_BLUE           CTERM_COLOR256(4)
#define CTERM_MAGENTA        CTERM_COLOR256(5)
#define CTERM_CYAN           CTERM_COLOR256(6)
#define CTERM_WHITE          CTERM_COLOR256(7)
#define CTERM_BRIGHT_BLACK   CTERM_COLOR256(8)
#define CTERM_BRIGHT_RED     CTERM_COLOR256(9)
#define CTERM_BRIGHT_GREEN   CTERM_COLOR256(10)
#define CTERM_BRIGHT_YELLOW  CTERM_COLOR256(11)
#define CTERM_BRIGHT_BLUE    CTERM_COLOR256(12)
#define CTERM_BRIGHT_MAGENTA CTERM_COLOR256(13)
#define CTERM_BRIGHT_CYAN    CTERM_COLOR256(14)
#define CTERM_BRIGHT_WHITE   CTERM_COLOR256(15)

/* Text attributes. Combine them with |. */
enum {
    CTERM_BOLD      = 1 << 0,
    CTERM_DIM       = 1 << 1,
    CTERM_ITALIC    = 1 << 2,
    CTERM_UNDERLINE = 1 << 3,
    CTERM_BLINK     = 1 << 4,
    CTERM_REVERSE   = 1 << 5, /* swap foreground and background */
    CTERM_STRIKE    = 1 << 6
};

typedef struct cterm_style {
    cterm_color fg, bg;
    uint32_t attrs;
} cterm_style;

/* For example: cterm_style_make(CTERM_RED, CTERM_DEFAULT, CTERM_BOLD) */
CTERM_API cterm_style cterm_style_make(cterm_color fg, cterm_color bg, uint32_t attrs);

/* The terminal's normal text style. */
CTERM_API cterm_style cterm_plain(void);

/* --- Rectangles ---------------------------------------------------------- */

typedef struct cterm_rect {
    int x, y; /* top-left cell; (0, 0) is the top-left of the buffer */
    int w, h; /* size in cells */
} cterm_rect;

CTERM_API cterm_rect cterm_rect_make(int x, int y, int w, int h);

/* The area two rectangles share. Its size is 0 if they don't overlap. */
CTERM_API cterm_rect cterm_rect_intersect(cterm_rect a, cterm_rect b);

/* 1 if the cell (x, y) is inside the rectangle, otherwise 0. */
CTERM_API int cterm_rect_contains(cterm_rect r, int x, int y);

/* The rectangle shrunk by `n` cells on every side. */
CTERM_API cterm_rect cterm_rect_inset(cterm_rect r, int n);

/* --- Cell buffers -------------------------------------------------------- */

/* How many combining marks fit on one character. Marks past this are dropped.
 * If you change it, define it the same way in every file. */
#ifndef CTERM_MAX_MARKS
  #define CTERM_MAX_MARKS 3
#endif

/* One terminal cell. `ch` is 0 in the right half of a wide character. */
typedef struct cterm_cell {
    uint32_t ch;
    uint32_t marks[CTERM_MAX_MARKS]; /* combining marks on ch; 0 means unused */
    cterm_style style;
} cterm_cell;

/* A grid of cells to draw on. The screen is a buffer (see cterm_screen), and you
 * can create your own to test drawing code without a terminal. */
typedef struct cterm_buffer {
    int width, height;
    cterm_cell *cells;   /* width * height cells, row by row */
    cterm_rect clip;     /* drawing outside this area is ignored */
    int cursor_visible;  /* see cterm_show_cursor */
    int cursor_x, cursor_y;
} cterm_buffer;

/* Creates a blank buffer. Returns NULL if out of memory. */
CTERM_API cterm_buffer *cterm_buffer_new(int width, int height);
CTERM_API void cterm_buffer_free(cterm_buffer *buffer);

/* Changes the size and blanks the contents. Returns 0, or -1 if out of memory,
 * in which case the buffer keeps its old size and contents. */
CTERM_API int cterm_buffer_resize(cterm_buffer *buffer, int width, int height);

/* Copies row `y` into `out` as UTF-8 and always NUL-terminates it. Returns the
 * length, or -1 if `y` is out of range. Handy for checking drawing in tests. */
CTERM_API int cterm_buffer_line(const cterm_buffer *buffer, int y, char *out, int cap);

/* --- Drawing ------------------------------------------------------------- */

/* The buffer's whole area. */
CTERM_API cterm_rect cterm_bounds(const cterm_buffer *buffer);

/* Blanks the buffer, removes the clip area and hides the cursor. Most apps call
 * this at the start of each frame and then draw everything again. */
CTERM_API void cterm_clear(cterm_buffer *buffer);

/* Draws one character. Combining marks attach to the character on the left.
 * Control characters are drawn as U+FFFD, so untrusted text can't sneak escape
 * sequences to the terminal. */
CTERM_API void cterm_put(cterm_buffer *buffer, int x, int y, uint32_t codepoint, cterm_style style);

/* Draws UTF-8 text on one line, and returns the x position just after it, so
 * calls can be chained. There's no wrapping: anything too long gets clipped. */
CTERM_API int cterm_text(cterm_buffer *buffer, int x, int y, cterm_style style, const char *utf8);

/* The same, for text that's `length` bytes long and may not be NUL-terminated. */
CTERM_API int cterm_textn(cterm_buffer *buffer, int x, int y, cterm_style style, const char *utf8, size_t length);

/* Fills a rectangle with one character. */
CTERM_API void cterm_fill(cterm_buffer *buffer, cterm_rect area, uint32_t codepoint, cterm_style style);

/* Limits drawing to `area` (it can't grow past the buffer). Returns the previous
 * clip area, so you can put it back when you're done:
 *     cterm_rect old = cterm_set_clip(s, panel);
 *     ...draw...
 *     cterm_set_clip(s, old);                                                 */
CTERM_API cterm_rect cterm_set_clip(cterm_buffer *buffer, cterm_rect area);

/* Shows the terminal's cursor at (x, y) once this buffer is presented. */
CTERM_API void cterm_show_cursor(cterm_buffer *buffer, int x, int y);
CTERM_API void cterm_hide_cursor(cterm_buffer *buffer);

/* --- Screen -------------------------------------------------------------- */

/* The buffer you draw on to change the screen. It's NULL before cterm_init().
 * The pointer stays valid until cterm_shutdown(), even across resizes. */
CTERM_API cterm_buffer *cterm_screen(void);

/* Sends what you've drawn to the terminal, writing only the cells that changed.
 * Returns 0, or -1 if the write failed. */
CTERM_API int cterm_present(void);

/* Makes the next cterm_present() repaint everything. Useful if something else
 * has scribbled on the screen. Apps often bind this to Ctrl+L. */
CTERM_API void cterm_redraw(void);

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

#include <stdlib.h>
#include <string.h>

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
  #include <sys/ioctl.h>
  #include <termios.h>
  #include <unistd.h>
#endif

/* --- Memory --------------------------------------------------------------- */

/* Define all three before the implementation to use your own allocator. */
#ifndef CTERM_MALLOC
  #define CTERM_MALLOC(size)        malloc(size)
  #define CTERM_REALLOC(ptr, size)  realloc(ptr, size)
  #define CTERM_FREE(ptr)           free(ptr)
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
 * UNICODE
 *
 * Terminals lay text out in a grid, so cterm has to know how many cells each
 * character takes, and has to agree with the terminal about it. The system's
 * wcwidth() can't be used: Windows doesn't have one, and the answers elsewhere
 * depend on the OS version and locale. So cterm ships its own table.
 * ========================================================================= */

#define CTERM__REPLACEMENT 0xFFFDu /* U+FFFD, drawn in place of anything invalid */

/* Decodes one UTF-8 character from `s` (`n` bytes available).
 *
 * Returns the number of bytes used, and stores the code point in *out. Invalid
 * bytes give U+FFFD and consume one byte, so decoding always moves forward.
 * Returns 0 if `s` is the valid start of a character that needs more bytes than
 * `n`, which lets the input parser wait for the rest of it to arrive.
 *
 * It rejects overlong forms, surrogates and values above U+10FFFF, as the UTF-8
 * standard requires. */
static int cterm__utf8_decode(const unsigned char *s, size_t n, uint32_t *out) {
    unsigned char b0;
    int need, i;
    uint32_t cp;

    if (n == 0) return 0;
    b0 = s[0];

    if (b0 < 0x80) {
        *out = b0;
        return 1;
    } else if (b0 >= 0xC2 && b0 <= 0xDF) {
        need = 2;
        cp = b0 & 0x1Fu;
    } else if (b0 >= 0xE0 && b0 <= 0xEF) {
        need = 3;
        cp = b0 & 0x0Fu;
    } else if (b0 >= 0xF0 && b0 <= 0xF4) {
        need = 4;
        cp = b0 & 0x07u;
    } else {
        *out = CTERM__REPLACEMENT;
        return 1;
    }

    for (i = 1; i < need; i++) {
        unsigned char b;
        if ((size_t)i >= n) return 0;
        b = s[i];
        if ((b & 0xC0) != 0x80 ||
            (i == 1 && ((b0 == 0xE0 && b < 0xA0) ||  /* overlong 3-byte form */
                        (b0 == 0xED && b > 0x9F) ||  /* surrogates */
                        (b0 == 0xF0 && b < 0x90) ||  /* overlong 4-byte form */
                        (b0 == 0xF4 && b > 0x8F)))) { /* above U+10FFFF */
            *out = CTERM__REPLACEMENT;
            return 1;
        }
        cp = (cp << 6) | (b & 0x3Fu);
    }
    *out = cp;
    return need;
}

/* Writes a code point as UTF-8 into `out`, which needs room for 4 bytes, and
 * returns the number of bytes written. */
static int cterm__utf8_encode(uint32_t cp, char *out) {
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

typedef struct cterm__range {
    uint32_t first, last;
} cterm__range;

/* BEGIN GENERATED WIDTH TABLES (tools/gen_width.py) */
/* Unicode 15.1.0. Don't edit by hand: see tools/gen_width.py. */
static const cterm__range cterm__zero_width[] = {
    {0x300,0x36F}, {0x483,0x489}, {0x591,0x5BD}, {0x5BF,0x5BF}, {0x5C1,0x5C2},
    {0x5C4,0x5C5}, {0x5C7,0x5C7}, {0x610,0x61A}, {0x61C,0x61C}, {0x64B,0x65F},
    {0x670,0x670}, {0x6D6,0x6DC}, {0x6DF,0x6E4}, {0x6E7,0x6E8}, {0x6EA,0x6ED},
    {0x711,0x711}, {0x730,0x74A}, {0x7A6,0x7B0}, {0x7EB,0x7F3}, {0x7FD,0x7FD},
    {0x816,0x819}, {0x81B,0x823}, {0x825,0x827}, {0x829,0x82D}, {0x859,0x85B},
    {0x898,0x89F}, {0x8CA,0x8E1}, {0x8E3,0x902}, {0x93A,0x93A}, {0x93C,0x93C},
    {0x941,0x948}, {0x94D,0x94D}, {0x951,0x957}, {0x962,0x963}, {0x981,0x981},
    {0x9BC,0x9BC}, {0x9C1,0x9C4}, {0x9CD,0x9CD}, {0x9E2,0x9E3}, {0x9FE,0x9FE},
    {0xA01,0xA02}, {0xA3C,0xA3C}, {0xA41,0xA42}, {0xA47,0xA48}, {0xA4B,0xA4D},
    {0xA51,0xA51}, {0xA70,0xA71}, {0xA75,0xA75}, {0xA81,0xA82}, {0xABC,0xABC},
    {0xAC1,0xAC5}, {0xAC7,0xAC8}, {0xACD,0xACD}, {0xAE2,0xAE3}, {0xAFA,0xAFF},
    {0xB01,0xB01}, {0xB3C,0xB3C}, {0xB3F,0xB3F}, {0xB41,0xB44}, {0xB4D,0xB4D},
    {0xB55,0xB56}, {0xB62,0xB63}, {0xB82,0xB82}, {0xBC0,0xBC0}, {0xBCD,0xBCD},
    {0xC00,0xC00}, {0xC04,0xC04}, {0xC3C,0xC3C}, {0xC3E,0xC40}, {0xC46,0xC48},
    {0xC4A,0xC4D}, {0xC55,0xC56}, {0xC62,0xC63}, {0xC81,0xC81}, {0xCBC,0xCBC},
    {0xCBF,0xCBF}, {0xCC6,0xCC6}, {0xCCC,0xCCD}, {0xCE2,0xCE3}, {0xD00,0xD01},
    {0xD3B,0xD3C}, {0xD41,0xD44}, {0xD4D,0xD4D}, {0xD62,0xD63}, {0xD81,0xD81},
    {0xDCA,0xDCA}, {0xDD2,0xDD4}, {0xDD6,0xDD6}, {0xE31,0xE31}, {0xE34,0xE3A},
    {0xE47,0xE4E}, {0xEB1,0xEB1}, {0xEB4,0xEBC}, {0xEC8,0xECE}, {0xF18,0xF19},
    {0xF35,0xF35}, {0xF37,0xF37}, {0xF39,0xF39}, {0xF71,0xF7E}, {0xF80,0xF84},
    {0xF86,0xF87}, {0xF8D,0xF97}, {0xF99,0xFBC}, {0xFC6,0xFC6}, {0x102D,0x1030},
    {0x1032,0x1037}, {0x1039,0x103A}, {0x103D,0x103E}, {0x1058,0x1059}, {0x105E,0x1060},
    {0x1071,0x1074}, {0x1082,0x1082}, {0x1085,0x1086}, {0x108D,0x108D}, {0x109D,0x109D},
    {0x1160,0x11FF}, {0x135D,0x135F}, {0x1712,0x1714}, {0x1732,0x1733}, {0x1752,0x1753},
    {0x1772,0x1773}, {0x17B4,0x17B5}, {0x17B7,0x17BD}, {0x17C6,0x17C6}, {0x17C9,0x17D3},
    {0x17DD,0x17DD}, {0x180B,0x180F}, {0x1885,0x1886}, {0x18A9,0x18A9}, {0x1920,0x1922},
    {0x1927,0x1928}, {0x1932,0x1932}, {0x1939,0x193B}, {0x1A17,0x1A18}, {0x1A1B,0x1A1B},
    {0x1A56,0x1A56}, {0x1A58,0x1A5E}, {0x1A60,0x1A60}, {0x1A62,0x1A62}, {0x1A65,0x1A6C},
    {0x1A73,0x1A7C}, {0x1A7F,0x1A7F}, {0x1AB0,0x1ACE}, {0x1B00,0x1B03}, {0x1B34,0x1B34},
    {0x1B36,0x1B3A}, {0x1B3C,0x1B3C}, {0x1B42,0x1B42}, {0x1B6B,0x1B73}, {0x1B80,0x1B81},
    {0x1BA2,0x1BA5}, {0x1BA8,0x1BA9}, {0x1BAB,0x1BAD}, {0x1BE6,0x1BE6}, {0x1BE8,0x1BE9},
    {0x1BED,0x1BED}, {0x1BEF,0x1BF1}, {0x1C2C,0x1C33}, {0x1C36,0x1C37}, {0x1CD0,0x1CD2},
    {0x1CD4,0x1CE0}, {0x1CE2,0x1CE8}, {0x1CED,0x1CED}, {0x1CF4,0x1CF4}, {0x1CF8,0x1CF9},
    {0x1DC0,0x1DFF}, {0x200B,0x200F}, {0x2028,0x202E}, {0x2060,0x2064}, {0x2066,0x206F},
    {0x20D0,0x20F0}, {0x2CEF,0x2CF1}, {0x2D7F,0x2D7F}, {0x2DE0,0x2DFF}, {0x302A,0x302D},
    {0x3099,0x309A}, {0xA66F,0xA672}, {0xA674,0xA67D}, {0xA69E,0xA69F}, {0xA6F0,0xA6F1},
    {0xA802,0xA802}, {0xA806,0xA806}, {0xA80B,0xA80B}, {0xA825,0xA826}, {0xA82C,0xA82C},
    {0xA8C4,0xA8C5}, {0xA8E0,0xA8F1}, {0xA8FF,0xA8FF}, {0xA926,0xA92D}, {0xA947,0xA951},
    {0xA980,0xA982}, {0xA9B3,0xA9B3}, {0xA9B6,0xA9B9}, {0xA9BC,0xA9BD}, {0xA9E5,0xA9E5},
    {0xAA29,0xAA2E}, {0xAA31,0xAA32}, {0xAA35,0xAA36}, {0xAA43,0xAA43}, {0xAA4C,0xAA4C},
    {0xAA7C,0xAA7C}, {0xAAB0,0xAAB0}, {0xAAB2,0xAAB4}, {0xAAB7,0xAAB8}, {0xAABE,0xAABF},
    {0xAAC1,0xAAC1}, {0xAAEC,0xAAED}, {0xAAF6,0xAAF6}, {0xABE5,0xABE5}, {0xABE8,0xABE8},
    {0xABED,0xABED}, {0xD7B0,0xD7FF}, {0xFB1E,0xFB1E}, {0xFE00,0xFE0F}, {0xFE20,0xFE2F},
    {0xFEFF,0xFEFF}, {0xFFF9,0xFFFB}, {0x101FD,0x101FD}, {0x102E0,0x102E0}, {0x10376,0x1037A},
    {0x10A01,0x10A03}, {0x10A05,0x10A06}, {0x10A0C,0x10A0F}, {0x10A38,0x10A3A}, {0x10A3F,0x10A3F},
    {0x10AE5,0x10AE6}, {0x10D24,0x10D27}, {0x10EAB,0x10EAC}, {0x10EFD,0x10EFF}, {0x10F46,0x10F50},
    {0x10F82,0x10F85}, {0x11001,0x11001}, {0x11038,0x11046}, {0x11070,0x11070}, {0x11073,0x11074},
    {0x1107F,0x11081}, {0x110B3,0x110B6}, {0x110B9,0x110BA}, {0x110C2,0x110C2}, {0x11100,0x11102},
    {0x11127,0x1112B}, {0x1112D,0x11134}, {0x11173,0x11173}, {0x11180,0x11181}, {0x111B6,0x111BE},
    {0x111C9,0x111CC}, {0x111CF,0x111CF}, {0x1122F,0x11231}, {0x11234,0x11234}, {0x11236,0x11237},
    {0x1123E,0x1123E}, {0x11241,0x11241}, {0x112DF,0x112DF}, {0x112E3,0x112EA}, {0x11300,0x11301},
    {0x1133B,0x1133C}, {0x11340,0x11340}, {0x11366,0x1136C}, {0x11370,0x11374}, {0x11438,0x1143F},
    {0x11442,0x11444}, {0x11446,0x11446}, {0x1145E,0x1145E}, {0x114B3,0x114B8}, {0x114BA,0x114BA},
    {0x114BF,0x114C0}, {0x114C2,0x114C3}, {0x115B2,0x115B5}, {0x115BC,0x115BD}, {0x115BF,0x115C0},
    {0x115DC,0x115DD}, {0x11633,0x1163A}, {0x1163D,0x1163D}, {0x1163F,0x11640}, {0x116AB,0x116AB},
    {0x116AD,0x116AD}, {0x116B0,0x116B5}, {0x116B7,0x116B7}, {0x1171D,0x1171F}, {0x11722,0x11725},
    {0x11727,0x1172B}, {0x1182F,0x11837}, {0x11839,0x1183A}, {0x1193B,0x1193C}, {0x1193E,0x1193E},
    {0x11943,0x11943}, {0x119D4,0x119D7}, {0x119DA,0x119DB}, {0x119E0,0x119E0}, {0x11A01,0x11A0A},
    {0x11A33,0x11A38}, {0x11A3B,0x11A3E}, {0x11A47,0x11A47}, {0x11A51,0x11A56}, {0x11A59,0x11A5B},
    {0x11A8A,0x11A96}, {0x11A98,0x11A99}, {0x11C30,0x11C36}, {0x11C38,0x11C3D}, {0x11C3F,0x11C3F},
    {0x11C92,0x11CA7}, {0x11CAA,0x11CB0}, {0x11CB2,0x11CB3}, {0x11CB5,0x11CB6}, {0x11D31,0x11D36},
    {0x11D3A,0x11D3A}, {0x11D3C,0x11D3D}, {0x11D3F,0x11D45}, {0x11D47,0x11D47}, {0x11D90,0x11D91},
    {0x11D95,0x11D95}, {0x11D97,0x11D97}, {0x11EF3,0x11EF4}, {0x11F00,0x11F01}, {0x11F36,0x11F3A},
    {0x11F40,0x11F40}, {0x11F42,0x11F42}, {0x13430,0x13440}, {0x13447,0x13455}, {0x16AF0,0x16AF4},
    {0x16B30,0x16B36}, {0x16F4F,0x16F4F}, {0x16F8F,0x16F92}, {0x16FE4,0x16FE4}, {0x1BC9D,0x1BC9E},
    {0x1BCA0,0x1BCA3}, {0x1CF00,0x1CF2D}, {0x1CF30,0x1CF46}, {0x1D167,0x1D169}, {0x1D173,0x1D182},
    {0x1D185,0x1D18B}, {0x1D1AA,0x1D1AD}, {0x1D242,0x1D244}, {0x1DA00,0x1DA36}, {0x1DA3B,0x1DA6C},
    {0x1DA75,0x1DA75}, {0x1DA84,0x1DA84}, {0x1DA9B,0x1DA9F}, {0x1DAA1,0x1DAAF}, {0x1E000,0x1E006},
    {0x1E008,0x1E018}, {0x1E01B,0x1E021}, {0x1E023,0x1E024}, {0x1E026,0x1E02A}, {0x1E08F,0x1E08F},
    {0x1E130,0x1E136}, {0x1E2AE,0x1E2AE}, {0x1E2EC,0x1E2EF}, {0x1E4EC,0x1E4EF}, {0x1E8D0,0x1E8D6},
    {0x1E944,0x1E94A}, {0xE0001,0xE0001}, {0xE0020,0xE007F}, {0xE0100,0xE01EF},
};
static const cterm__range cterm__double_width[] = {
    {0x1100,0x115F}, {0x231A,0x231B}, {0x2329,0x232A}, {0x23E9,0x23EC}, {0x23F0,0x23F0},
    {0x23F3,0x23F3}, {0x25FD,0x25FE}, {0x2614,0x2615}, {0x2648,0x2653}, {0x267F,0x267F},
    {0x2693,0x2693}, {0x26A1,0x26A1}, {0x26AA,0x26AB}, {0x26BD,0x26BE}, {0x26C4,0x26C5},
    {0x26CE,0x26CE}, {0x26D4,0x26D4}, {0x26EA,0x26EA}, {0x26F2,0x26F3}, {0x26F5,0x26F5},
    {0x26FA,0x26FA}, {0x26FD,0x26FD}, {0x2705,0x2705}, {0x270A,0x270B}, {0x2728,0x2728},
    {0x274C,0x274C}, {0x274E,0x274E}, {0x2753,0x2755}, {0x2757,0x2757}, {0x2795,0x2797},
    {0x27B0,0x27B0}, {0x27BF,0x27BF}, {0x2B1B,0x2B1C}, {0x2B50,0x2B50}, {0x2B55,0x2B55},
    {0x2E80,0x2E99}, {0x2E9B,0x2EF3}, {0x2F00,0x2FD5}, {0x2FF0,0x3029}, {0x302E,0x303E},
    {0x3041,0x3096}, {0x309B,0x30FF}, {0x3105,0x312F}, {0x3131,0x318E}, {0x3190,0x31E3},
    {0x31EF,0x321E}, {0x3220,0x3247}, {0x3250,0x4DBF}, {0x4E00,0xA48C}, {0xA490,0xA4C6},
    {0xA960,0xA97C}, {0xAC00,0xD7A3}, {0xF900,0xFAFF}, {0xFE10,0xFE19}, {0xFE30,0xFE52},
    {0xFE54,0xFE66}, {0xFE68,0xFE6B}, {0xFF01,0xFF60}, {0xFFE0,0xFFE6}, {0x16FE0,0x16FE3},
    {0x16FF0,0x16FF1}, {0x17000,0x187F7}, {0x18800,0x18CD5}, {0x18D00,0x18D08}, {0x1AFF0,0x1AFF3},
    {0x1AFF5,0x1AFFB}, {0x1AFFD,0x1AFFE}, {0x1B000,0x1B122}, {0x1B132,0x1B132}, {0x1B150,0x1B152},
    {0x1B155,0x1B155}, {0x1B164,0x1B167}, {0x1B170,0x1B2FB}, {0x1F004,0x1F004}, {0x1F0CF,0x1F0CF},
    {0x1F18E,0x1F18E}, {0x1F191,0x1F19A}, {0x1F200,0x1F202}, {0x1F210,0x1F23B}, {0x1F240,0x1F248},
    {0x1F250,0x1F251}, {0x1F260,0x1F265}, {0x1F300,0x1F320}, {0x1F32D,0x1F335}, {0x1F337,0x1F37C},
    {0x1F37E,0x1F393}, {0x1F3A0,0x1F3CA}, {0x1F3CF,0x1F3D3}, {0x1F3E0,0x1F3F0}, {0x1F3F4,0x1F3F4},
    {0x1F3F8,0x1F43E}, {0x1F440,0x1F440}, {0x1F442,0x1F4FC}, {0x1F4FF,0x1F53D}, {0x1F54B,0x1F54E},
    {0x1F550,0x1F567}, {0x1F57A,0x1F57A}, {0x1F595,0x1F596}, {0x1F5A4,0x1F5A4}, {0x1F5FB,0x1F64F},
    {0x1F680,0x1F6C5}, {0x1F6CC,0x1F6CC}, {0x1F6D0,0x1F6D2}, {0x1F6D5,0x1F6D7}, {0x1F6DC,0x1F6DF},
    {0x1F6EB,0x1F6EC}, {0x1F6F4,0x1F6FC}, {0x1F7E0,0x1F7EB}, {0x1F7F0,0x1F7F0}, {0x1F90C,0x1F93A},
    {0x1F93C,0x1F945}, {0x1F947,0x1F9FF}, {0x1FA70,0x1FA7C}, {0x1FA80,0x1FA88}, {0x1FA90,0x1FABD},
    {0x1FABF,0x1FAC5}, {0x1FACE,0x1FADB}, {0x1FAE0,0x1FAE8}, {0x1FAF0,0x1FAF8}, {0x20000,0x2FFFD},
    {0x30000,0x3FFFD},
};
/* END GENERATED WIDTH TABLES */

/* Binary search through a sorted list of ranges. */
static int cterm__in_ranges(uint32_t cp, const cterm__range *table, int count) {
    int low = 0, high = count - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (cp < table[mid].first) high = mid - 1;
        else if (cp > table[mid].last) low = mid + 1;
        else return 1;
    }
    return 0;
}

CTERM_API int cterm_char_width(uint32_t cp) {
    /* Nothing below U+0300 is zero-width or wide, and that covers most text. */
    if (cp < 0x300) return 1;
    if (cterm__in_ranges(cp, cterm__zero_width, (int)(sizeof cterm__zero_width / sizeof cterm__zero_width[0]))) return 0;
    if (cterm__in_ranges(cp, cterm__double_width, (int)(sizeof cterm__double_width / sizeof cterm__double_width[0]))) return 2;
    return 1;
}

CTERM_API int cterm_text_width(const char *utf8) {
    const unsigned char *s = (const unsigned char *)utf8;
    size_t n = 0;
    int total = 0;
    while (s[n]) n++;
    while (n > 0) {
        uint32_t cp = CTERM__REPLACEMENT;
        int used = cterm__utf8_decode(s, n, &cp);
        if (used == 0) { /* string ended partway through a character */
            cp = CTERM__REPLACEMENT;
            used = 1;
        }
        total += cterm_char_width(cp);
        s += used;
        n -= (size_t)used;
    }
    return total;
}

/* ========================================================================= *
 * STYLES AND RECTANGLES
 * ========================================================================= */

CTERM_API cterm_style cterm_style_make(cterm_color fg, cterm_color bg, uint32_t attrs) {
    cterm_style style;
    style.fg = fg;
    style.bg = bg;
    style.attrs = attrs;
    return style;
}

CTERM_API cterm_style cterm_plain(void) {
    return cterm_style_make(CTERM_DEFAULT, CTERM_DEFAULT, 0);
}

static int cterm__style_equal(cterm_style a, cterm_style b) {
    return a.fg == b.fg && a.bg == b.bg && a.attrs == b.attrs;
}

CTERM_API cterm_rect cterm_rect_make(int x, int y, int w, int h) {
    cterm_rect r;
    r.x = x;
    r.y = y;
    r.w = w > 0 ? w : 0;
    r.h = h > 0 ? h : 0;
    return r;
}

CTERM_API cterm_rect cterm_rect_intersect(cterm_rect a, cterm_rect b) {
    int left   = a.x > b.x ? a.x : b.x;
    int top    = a.y > b.y ? a.y : b.y;
    int right  = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w;
    int bottom = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    return cterm_rect_make(left, top, right - left, bottom - top);
}

CTERM_API int cterm_rect_contains(cterm_rect r, int x, int y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

CTERM_API cterm_rect cterm_rect_inset(cterm_rect r, int n) {
    return cterm_rect_make(r.x + n, r.y + n, r.w - 2 * n, r.h - 2 * n);
}

/* ========================================================================= *
 * CELL BUFFERS AND DRAWING
 *
 * The one rule everything here protects: a wide character always occupies two
 * cells, a "lead" cell holding the character and a "continuation" cell to its
 * right with ch == 0. There's never a lone half of a wide character. If you
 * overwrite one half, the other half turns into a space, which is also what a
 * real terminal does. The renderer relies on this.
 * ========================================================================= */

static cterm_cell cterm__blank_cell(void) {
    cterm_cell cell;
    memset(&cell, 0, sizeof cell);
    cell.ch = ' ';
    return cell;
}

/* Allocates the cell array for a size, without touching any buffer. */
static cterm_cell *cterm__cells_new(int width, int height) {
    cterm_cell blank = cterm__blank_cell();
    cterm_cell *cells;
    size_t count = (size_t)width * (size_t)height, i;
    /* Allocating at least one cell means malloc(0) can't look like a failure. */
    if (height != 0 && count / (size_t)height != (size_t)width) return NULL;
    if (count > ((size_t)-1) / sizeof(cterm_cell) - 1) return NULL;
    cells = (cterm_cell *)CTERM_MALLOC((count ? count : 1) * sizeof(cterm_cell));
    if (!cells) return NULL;
    for (i = 0; i < count; i++) cells[i] = blank;
    return cells;
}

CTERM_API int cterm_buffer_resize(cterm_buffer *buffer, int width, int height) {
    cterm_cell *cells;
    if (width < 0) width = 0;
    if (height < 0) height = 0;
    cells = cterm__cells_new(width, height);
    if (!cells) return cterm__fail("out of memory");
    CTERM_FREE(buffer->cells);
    buffer->cells = cells;
    buffer->width = width;
    buffer->height = height;
    buffer->clip = cterm_rect_make(0, 0, width, height);
    buffer->cursor_visible = 0;
    buffer->cursor_x = 0;
    buffer->cursor_y = 0;
    return 0;
}

CTERM_API cterm_buffer *cterm_buffer_new(int width, int height) {
    cterm_buffer *buffer = (cterm_buffer *)CTERM_MALLOC(sizeof(cterm_buffer));
    if (!buffer) {
        cterm__fail("out of memory");
        return NULL;
    }
    memset(buffer, 0, sizeof *buffer);
    if (cterm_buffer_resize(buffer, width, height) != 0) {
        CTERM_FREE(buffer);
        return NULL;
    }
    return buffer;
}

CTERM_API void cterm_buffer_free(cterm_buffer *buffer) {
    if (!buffer) return;
    CTERM_FREE(buffer->cells);
    CTERM_FREE(buffer);
}

CTERM_API cterm_rect cterm_bounds(const cterm_buffer *buffer) {
    return cterm_rect_make(0, 0, buffer->width, buffer->height);
}

CTERM_API cterm_rect cterm_set_clip(cterm_buffer *buffer, cterm_rect area) {
    cterm_rect previous = buffer->clip;
    buffer->clip = cterm_rect_intersect(area, cterm_bounds(buffer));
    return previous;
}

CTERM_API void cterm_show_cursor(cterm_buffer *buffer, int x, int y) {
    /* A cursor can't be shown outside the visible area, so hide it instead. */
    if (!cterm_rect_contains(buffer->clip, x, y)) {
        buffer->cursor_visible = 0;
        return;
    }
    buffer->cursor_visible = 1;
    buffer->cursor_x = x;
    buffer->cursor_y = y;
}

CTERM_API void cterm_hide_cursor(cterm_buffer *buffer) {
    buffer->cursor_visible = 0;
}

CTERM_API void cterm_clear(cterm_buffer *buffer) {
    cterm_cell blank = cterm__blank_cell();
    size_t count = (size_t)buffer->width * (size_t)buffer->height, i;
    for (i = 0; i < count; i++) buffer->cells[i] = blank;
    buffer->clip = cterm_bounds(buffer);
    buffer->cursor_visible = 0;
}

static cterm_cell *cterm__cell_at(const cterm_buffer *buffer, int x, int y) {
    return &buffer->cells[(size_t)y * (size_t)buffer->width + (size_t)x];
}

/* Turns a cell into a space, keeping its colours. */
static void cterm__blank_keep_style(cterm_cell *cell) {
    cterm_style style = cell->style;
    *cell = cterm__blank_cell();
    cell->style = style;
}

/* Called just before the cell at (x, y) is overwritten. If it's part of a wide
 * character, the other half can't stay, so it's blanked. That can touch a cell
 * outside the clip area, because keeping wide characters whole matters more. */
static void cterm__split_wide(cterm_buffer *buffer, int x, int y) {
    if (cterm__cell_at(buffer, x, y)->ch == 0 && x > 0) {
        cterm__blank_keep_style(cterm__cell_at(buffer, x - 1, y));
    }
    if (x + 1 < buffer->width && cterm__cell_at(buffer, x + 1, y)->ch == 0) {
        cterm__blank_keep_style(cterm__cell_at(buffer, x + 1, y));
    }
}

/* Writes a character whose cells are already known to be inside the clip area. */
static void cterm__write_cell(cterm_buffer *buffer, int x, int y, uint32_t cp, int width, cterm_style style) {
    cterm_cell *cell;

    cterm__split_wide(buffer, x, y);
    if (width == 2) cterm__split_wide(buffer, x + 1, y);

    cell = cterm__cell_at(buffer, x, y);
    memset(cell, 0, sizeof *cell);
    cell->ch = cp;
    cell->style = style;

    if (width == 2) {
        cell = cterm__cell_at(buffer, x + 1, y);
        memset(cell, 0, sizeof *cell); /* ch == 0 marks the continuation cell */
        cell->style = style;
    }
}

/* Replaces anything that isn't safe or valid to send to a terminal. */
static uint32_t cterm__sanitize(uint32_t cp) {
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0)) return CTERM__REPLACEMENT; /* control codes */
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return CTERM__REPLACEMENT;
    return cp;
}

/* Adds a combining mark to the character at (x, y). */
static void cterm__attach_mark(cterm_buffer *buffer, int x, int y, uint32_t mark) {
    cterm_cell *cell;
    int i;
    if (!cterm_rect_contains(buffer->clip, x, y)) return;
    cell = cterm__cell_at(buffer, x, y);
    if (cell->ch == 0 && x > 0) cell = cterm__cell_at(buffer, x - 1, y); /* use the wide lead */
    for (i = 0; i < CTERM_MAX_MARKS; i++) {
        if (cell->marks[i] == 0) {
            cell->marks[i] = mark;
            return;
        }
    }
}

CTERM_API void cterm_put(cterm_buffer *buffer, int x, int y, uint32_t cp, cterm_style style) {
    int width;
    cp = cterm__sanitize(cp);
    width = cterm_char_width(cp);

    if (width == 0) {
        cterm__attach_mark(buffer, x - 1, y, cp);
    } else if (width == 1) {
        if (cterm_rect_contains(buffer->clip, x, y)) cterm__write_cell(buffer, x, y, cp, 1, style);
    } else {
        int left  = cterm_rect_contains(buffer->clip, x, y);
        int right = cterm_rect_contains(buffer->clip, x + 1, y);
        if (left && right) {
            cterm__write_cell(buffer, x, y, cp, 2, style);
        } else if (left) {
            cterm__write_cell(buffer, x, y, ' ', 1, style); /* half a character can't be drawn */
        } else if (right) {
            cterm__write_cell(buffer, x + 1, y, ' ', 1, style);
        }
    }
}

CTERM_API int cterm_textn(cterm_buffer *buffer, int x, int y, cterm_style style, const char *utf8, size_t length) {
    const unsigned char *s = (const unsigned char *)utf8;
    int have_base = 0; /* a mark needs a character from this string to attach to */

    while (length > 0) {
        uint32_t cp = CTERM__REPLACEMENT;
        int used = cterm__utf8_decode(s, length, &cp);
        int width;
        if (used == 0) { /* string ended partway through a character */
            cp = CTERM__REPLACEMENT;
            used = 1;
        }
        s += used;
        length -= (size_t)used;

        cp = cterm__sanitize(cp);
        width = cterm_char_width(cp);
        if (width == 0 && !have_base) continue;
        cterm_put(buffer, x, y, cp, style);
        x += width;
        have_base = 1;
    }
    return x;
}

CTERM_API int cterm_text(cterm_buffer *buffer, int x, int y, cterm_style style, const char *utf8) {
    return cterm_textn(buffer, x, y, style, utf8, strlen(utf8));
}

CTERM_API void cterm_fill(cterm_buffer *buffer, cterm_rect area, uint32_t cp, cterm_style style) {
    int x, y, step;
    area = cterm_rect_intersect(area, buffer->clip);
    step = cterm_char_width(cterm__sanitize(cp));
    if (step == 0) return; /* a mark alone can't fill anything */
    for (y = area.y; y < area.y + area.h; y++) {
        for (x = area.x; x < area.x + area.w; x += step) {
            cterm_put(buffer, x, y, cp, style);
        }
    }
}

CTERM_API int cterm_buffer_line(const cterm_buffer *buffer, int y, char *out, int cap) {
    int x, m, len = 0;
    if (y < 0 || y >= buffer->height || cap <= 0) return -1;
    for (x = 0; x < buffer->width; x++) {
        const cterm_cell *cell = cterm__cell_at(buffer, x, y);
        char bytes[4];
        int n;
        if (cell->ch == 0) continue;
        n = cterm__utf8_encode(cell->ch, bytes);
        if (len + n >= cap) break;
        memcpy(out + len, bytes, (size_t)n);
        len += n;
        for (m = 0; m < CTERM_MAX_MARKS && cell->marks[m]; m++) {
            n = cterm__utf8_encode(cell->marks[m], bytes);
            if (len + n >= cap) break;
            memcpy(out + len, bytes, (size_t)n);
            len += n;
        }
    }
    out[len] = '\0';
    return len;
}

/* ========================================================================= *
 * RENDERER
 *
 * Turns "what the screen should look like" into terminal escape sequences.
 *
 * Two buffers are kept: `back` is what the app has drawn, and `front` is what's
 * on the terminal now. Comparing them gives the smallest update, which avoids
 * flicker and keeps things fast over SSH.
 *
 * Nothing here touches a terminal: it only appends bytes to a string. That means
 * tests can check the output directly (see tests/test_render.c).
 * ========================================================================= */

/* A growable byte string. If memory runs out, `failed` is set and further
 * appends are ignored, so callers can check once at the end. */
typedef struct cterm__sb {
    char *data;
    size_t len, cap;
    int failed;
} cterm__sb;

static void cterm__sb_append(cterm__sb *sb, const char *bytes, size_t n) {
    if (sb->failed) return;
    if (sb->len + n > sb->cap) {
        size_t cap = sb->cap ? sb->cap : 4096;
        char *data;
        while (cap < sb->len + n) cap *= 2;
        data = (char *)CTERM_REALLOC(sb->data, cap);
        if (!data) {
            sb->failed = 1;
            return;
        }
        sb->data = data;
        sb->cap = cap;
    }
    memcpy(sb->data + sb->len, bytes, n);
    sb->len += n;
}

static void cterm__sb_str(cterm__sb *sb, const char *text) {
    cterm__sb_append(sb, text, strlen(text));
}

static void cterm__sb_int(cterm__sb *sb, int value) {
    char digits[12];
    int i = (int)sizeof digits;
    unsigned magnitude = value < 0 ? 0u : (unsigned)value;
    do {
        digits[--i] = (char)('0' + magnitude % 10);
        magnitude /= 10;
    } while (magnitude > 0);
    cterm__sb_append(sb, digits + i, sizeof digits - (size_t)i);
}

static void cterm__sb_free(cterm__sb *sb) {
    CTERM_FREE(sb->data);
    memset(sb, 0, sizeof *sb);
}

/* What the renderer believes about the real terminal. A cursor position of -1
 * means "unknown", which forces the next move to be sent explicitly. */
typedef struct cterm__render_state {
    int cursor_x, cursor_y;
    int cursor_visible;
    int style_known;
    cterm_style style;
} cterm__render_state;

static void cterm__render_state_reset(cterm__render_state *state) {
    state->cursor_x = -1;
    state->cursor_y = -1;
    state->cursor_visible = 0;
    state->style_known = 0;
    state->style = cterm_plain();
}

/* Appends one colour to an SGR ("set style") sequence. */
static void cterm__sgr_color(cterm__sb *out, cterm_color color, int background) {
    uint32_t kind = color & 0xFF000000u;
    if (color == CTERM_DEFAULT) return; /* the reset at the start of the sequence covers it */
    if (kind == 0x01000000u) {
        cterm__sb_str(out, background ? ";48;5;" : ";38;5;");
        cterm__sb_int(out, (int)(color & 0xFFu));
    } else {
        cterm__sb_str(out, background ? ";48;2;" : ";38;2;");
        cterm__sb_int(out, (int)((color >> 16) & 0xFFu));
        cterm__sb_str(out, ";");
        cterm__sb_int(out, (int)((color >> 8) & 0xFFu));
        cterm__sb_str(out, ";");
        cterm__sb_int(out, (int)(color & 0xFFu));
    }
}

/* Sends a whole style. Every sequence starts with 0 (reset), so nothing can
 * carry over from the previous style by accident. */
static void cterm__sgr_style(cterm__sb *out, cterm_style style) {
    cterm__sb_str(out, "\x1b[0");
    if (style.attrs & CTERM_BOLD)      cterm__sb_str(out, ";1");
    if (style.attrs & CTERM_DIM)       cterm__sb_str(out, ";2");
    if (style.attrs & CTERM_ITALIC)    cterm__sb_str(out, ";3");
    if (style.attrs & CTERM_UNDERLINE) cterm__sb_str(out, ";4");
    if (style.attrs & CTERM_BLINK)     cterm__sb_str(out, ";5");
    if (style.attrs & CTERM_REVERSE)   cterm__sb_str(out, ";7");
    if (style.attrs & CTERM_STRIKE)    cterm__sb_str(out, ";9");
    cterm__sgr_color(out, style.fg, 0);
    cterm__sgr_color(out, style.bg, 1);
    cterm__sb_str(out, "m");
}

/* Moves the terminal's cursor, unless it's already there. */
static void cterm__render_move(cterm__sb *out, cterm__render_state *state, int x, int y) {
    if (state->cursor_x == x && state->cursor_y == y) return;
    cterm__sb_str(out, "\x1b[");
    cterm__sb_int(out, y + 1); /* terminals count from 1 */
    cterm__sb_str(out, ";");
    cterm__sb_int(out, x + 1);
    cterm__sb_str(out, "H");
    state->cursor_x = x;
    state->cursor_y = y;
}

static int cterm__cell_equal(const cterm_cell *a, const cterm_cell *b) {
    int i;
    if (a->ch != b->ch || !cterm__style_equal(a->style, b->style)) return 0;
    for (i = 0; i < CTERM_MAX_MARKS; i++) {
        if (a->marks[i] != b->marks[i]) return 0;
    }
    return 1;
}

/* Appends what's needed to make the terminal match `back`, and updates `front`
 * to match as well. `front` and `back` must be the same size.
 *
 * With `full` set, the screen is cleared first and everything is repainted,
 * which recovers from a resize or from anything else writing to the screen. */
static void cterm__render(cterm__sb *out, cterm_buffer *front, const cterm_buffer *back,
                          cterm__render_state *state, int full) {
    int x, y, painting = 0;

    if (full) {
        /* After this the screen is known to be blank, so the normal comparison
         * below only has to paint cells that aren't blank. */
        cterm__sb_str(out, "\x1b[?2026h\x1b[?25l\x1b[0m\x1b[2J");
        cterm__render_state_reset(state);
        state->style_known = 1;
        cterm_clear(front);
        painting = 1;
    }

    for (y = 0; y < back->height; y++) {
        for (x = 0; x < back->width; x++) {
            const cterm_cell *want = cterm__cell_at(back, x, y);
            cterm_cell *have = cterm__cell_at(front, x, y);
            int m;

            if (cterm__cell_equal(want, have)) continue;
            *have = *want;

            /* The right half of a wide character was drawn with its left half. */
            if (want->ch == 0) continue;

            if (!painting) {
                /* Synchronized output (mode 2026) asks the terminal to show this
                 * whole update at once. Terminals without it ignore the request.
                 * The cursor is hidden so it doesn't visibly jump around. */
                cterm__sb_str(out, "\x1b[?2026h");
                if (state->cursor_visible) cterm__sb_str(out, "\x1b[?25l");
                state->cursor_visible = 0;
                painting = 1;
            }

            cterm__render_move(out, state, x, y);

            if (!state->style_known || !cterm__style_equal(state->style, want->style)) {
                cterm__sgr_style(out, want->style);
                state->style = want->style;
                state->style_known = 1;
            }

            {
                char bytes[4];
                int n = cterm__utf8_encode(want->ch, bytes);
                int width = cterm_char_width(want->ch);
                cterm__sb_append(out, bytes, (size_t)n);
                for (m = 0; m < CTERM_MAX_MARKS && want->marks[m]; m++) {
                    n = cterm__utf8_encode(want->marks[m], bytes);
                    cterm__sb_append(out, bytes, (size_t)n);
                }

                /* Terminals don't all agree on the width of wide characters, emoji
                 * and marked text. Forgetting the position forces an explicit move
                 * next, so a disagreement can't push the rest of the row sideways. */
                if (width == 2 || want->ch >= 0x1F000 || want->marks[0]) {
                    state->cursor_x = -1;
                    state->cursor_y = -1;
                } else {
                    state->cursor_x = x + 1;
                }
            }
        }
    }

    /* Put the cursor where the app wants it. */
    if (back->cursor_visible) {
        if (!state->cursor_visible || state->cursor_x != back->cursor_x || state->cursor_y != back->cursor_y) {
            cterm__render_move(out, state, back->cursor_x, back->cursor_y);
            if (!state->cursor_visible) cterm__sb_str(out, "\x1b[?25h");
            state->cursor_visible = 1;
        }
    } else if (state->cursor_visible) {
        cterm__sb_str(out, "\x1b[?25l");
        state->cursor_visible = 0;
    }

    if (painting) cterm__sb_str(out, "\x1b[?2026l");
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
 *   cterm__plat_write      write all the bytes; 0 on success, -1 on failure
 *   cterm__plat_size       get the terminal's size in cells
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

/* Windows reports text as UTF-16, with characters above U+FFFF split into two
 * "surrogate" units that may arrive in separate records. The first half waits
 * here until the second shows up. */
static WCHAR cterm__high_surrogate;

static int cterm__win_put_utf16(unsigned char *out, WCHAR unit) {
    uint32_t cp = unit;
    if (unit >= 0xD800 && unit <= 0xDBFF) {
        cterm__high_surrogate = unit;
        return 0;
    }
    if (unit >= 0xDC00 && unit <= 0xDFFF) {
        if (!cterm__high_surrogate) return 0; /* stray half: drop it */
        cp = 0x10000 + (((uint32_t)cterm__high_surrogate - 0xD800) << 10) + ((uint32_t)unit - 0xDC00);
    }
    cterm__high_surrogate = 0;
    return cterm__utf8_encode(cp, (char *)out);
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

static int cterm__plat_write(const char *data, size_t n) {
    while (n > 0) {
        DWORD written = 0;
        DWORD chunk = n > 0x10000 ? 0x10000 : (DWORD)n;
        if (!WriteFile(cterm__hout, data, chunk, &written, NULL)) return -1;
        data += written;
        n -= written;
    }
    return 0;
}

static void cterm__plat_size(int *width, int *height) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    *width = 80;
    *height = 24;
    if (GetConsoleScreenBufferInfo(cterm__hout, &info)) {
        /* srWindow is the visible part, which is smaller than the scrollback. */
        *width  = info.srWindow.Right - info.srWindow.Left + 1;
        *height = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
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

static int cterm__plat_write(const char *data, size_t n) {
    while (n > 0) {
        ssize_t written = write(STDOUT_FILENO, data, n);
        if (written < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            return -1;
        }
        data += written;
        n -= (size_t)written;
    }
    return 0;
}

static void cterm__plat_size(int *width, int *height) {
    struct winsize size;
    *width = 80;
    *height = 24;
    /* Some environments report 0x0, so only trust a real answer. */
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0 && size.ws_row > 0) {
        *width = size.ws_col;
        *height = size.ws_row;
    }
}

#endif /* platform */

/* ========================================================================= *
 * LIFECYCLE
 * ========================================================================= */

/* Entering: alternate screen (so the user's shell history comes back when we
 * finish), no auto-wrap (so drawing the bottom-right cell can't scroll the
 * screen), hidden cursor, and bracketed paste. */
#define CTERM__ENTER_SEQUENCE "\x1b[?1049h\x1b[?7l\x1b[?25l\x1b[?2004h"

/* Leaving undoes everything, including modes that may not be on, because turning
 * off something that's already off is harmless. */
#define CTERM__LEAVE_SEQUENCE \
    "\x1b[?2026l\x1b[?2004l\x1b[?1006l\x1b[?1003l\x1b[?1002l\x1b[?1000l\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l"

/* All the state of the one terminal a process has. */
static struct {
    int active;
    cterm_buffer back, front;
    cterm__sb out;
    cterm__render_state render;
    int repaint; /* the next present must repaint everything */
} cterm__term;

/* Resizes both buffers to match the terminal. Returns 0 or -1. */
static int cterm__update_size(void) {
    int width, height;
    cterm__plat_size(&width, &height);
    if (width == cterm__term.back.width && height == cterm__term.back.height) return 0;
    if (cterm_buffer_resize(&cterm__term.back, width, height) != 0) return -1;
    if (cterm_buffer_resize(&cterm__term.front, width, height) != 0) return -1;
    cterm__term.repaint = 1;
    return 0;
}

CTERM_API int cterm_init(void) {
    /* Calling init twice must not save raw settings as the "normal" ones. */
    if (cterm__term.active) return 0;
    if (cterm__plat_init() != 0) return -1;

    if (cterm__update_size() != 0) {
        cterm__plat_shutdown();
        return -1;
    }
    cterm__render_state_reset(&cterm__term.render);
    cterm__term.repaint = 1;
    cterm__term.active = 1;
    cterm__plat_write(CTERM__ENTER_SEQUENCE, strlen(CTERM__ENTER_SEQUENCE));
    return 0;
}

CTERM_API void cterm_shutdown(void) {
    if (!cterm__term.active) return;
    cterm__term.active = 0;
    cterm__plat_write(CTERM__LEAVE_SEQUENCE, strlen(CTERM__LEAVE_SEQUENCE));
    cterm__plat_shutdown();

    /* Free everything, so a later cterm_init() starts from a clean slate. */
    CTERM_FREE(cterm__term.back.cells);
    CTERM_FREE(cterm__term.front.cells);
    cterm__sb_free(&cterm__term.out);
    memset(&cterm__term, 0, sizeof cterm__term);
}

CTERM_API cterm_buffer *cterm_screen(void) {
    return cterm__term.active ? &cterm__term.back : NULL;
}

CTERM_API void cterm_redraw(void) {
    cterm__term.repaint = 1;
}

CTERM_API int cterm_present(void) {
    if (!cterm__term.active) return cterm__fail("cterm_init has not been called");

    cterm__term.out.len = 0;
    cterm__render(&cterm__term.out, &cterm__term.front, &cterm__term.back, &cterm__term.render, cterm__term.repaint);
    cterm__term.repaint = 0;

    if (cterm__term.out.failed) {
        /* Part of the frame was lost, so repaint from scratch next time. */
        cterm__term.out.failed = 0;
        cterm__term.repaint = 1;
        return cterm__fail("out of memory");
    }
    if (cterm__term.out.len == 0) return 0;
    if (cterm__plat_write(cterm__term.out.data, cterm__term.out.len) != 0) {
        return cterm__fail("could not write to the terminal");
    }
    return 0;
}

CTERM_API int cterm_read(unsigned char *buf, int cap) {
    if (!cterm__term.active) return cterm__fail("cterm_init has not been called");
    return cterm__plat_read(buf, cap);
}

#endif /* CTERM_IMPLEMENTATION_DONE */
#endif /* CTERM_IMPLEMENTATION */
