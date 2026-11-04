/* cterm.h - a cross-platform, single-header terminal UI library for C and C++.
 *
 * HOW TO USE IT
 *
 * In exactly one source file, before any other #include:
 *
 *     #define CTERM_IMPLEMENTATION
 *     #include "cterm.h"
 *
 * Every other file just does #include "cterm.h". There's nothing else to build or
 * link. README.md is the full guide, and every function is documented where it's
 * declared in the PUBLIC API section below.
 *
 * OPTIONS
 *
 * Define these before including the header.
 *
 * In every file:
 *   CTERM_NO_UI            leave out the layout and widget layer
 *   CTERM_MAX_MARKS        combining marks per character (default 3)
 *   CTERM_UI_MAX_WIDGETS   focusable widgets per frame (default 128)
 *   CTERM_API              what goes in front of public functions
 *
 * Only where CTERM_IMPLEMENTATION is defined:
 *   CTERM_STATIC           make everything private to this file
 *   CTERM_MALLOC, CTERM_REALLOC, CTERM_FREE   your own allocator (all three)
 *   CTERM_ESC_TIMEOUT_MS   how long a lone ESC waits to be the Esc key (default 50)
 *   CTERM_PASTE_TIMEOUT_MS how long a stalled paste waits (default 500)
 *   CTERM_PASTE_MAX        the longest paste kept, in bytes (default 16 MiB)
 *
 * MAP OF THIS FILE
 *
 * To find a section, search for its name in capitals.
 *
 *   PUBLIC API                  everything an app can call
 *   IMPLEMENTATION              starts here
 *     UNICODE                   UTF-8 and character widths (generated tables)
 *     STYLES AND RECTANGLES     small helpers
 *     CELL BUFFERS AND DRAWING  the grid apps draw on
 *     RENDERER                  buffer -> escape sequences; no terminal needed
 *     INPUT PARSER              bytes -> events; no terminal needed
 *     PLATFORM LAYER            all the OS-specific code (POSIX and Windows)
 *     LIFECYCLE                 cterm_init, cterm_shutdown, cterm_present
 *     EVENT LOOP                cterm_poll: timing on top of the parser
 *     LAYOUT                    splitting rectangles
 *     WIDGET CORE               focus, and event routing
 *     TEXT INPUT, LIST, SCROLLABLE VIEW
 *
 * Licence: MIT, see the end of this file.
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

/* Puts the app in the background and hands the terminal back to the shell, which is
 * what Ctrl+Z normally does. cterm delivers Ctrl+Z as an ordinary key, so bind it
 * yourself if you want this. It returns once the user resumes the app (`fg`), and
 * then cterm_poll reports CTERM_EVENT_RESIZE so that you redraw.
 *
 * Returns 0, or -1 on Windows, which has no such feature. */
CTERM_API int cterm_suspend(void);

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

/* How many colours the terminal can show. cterm works this out at startup from the
 * environment (NO_COLOR, COLORTERM and TERM). You can always draw with any colour:
 * it's converted to the nearest one the terminal can show. */
typedef enum cterm_color_mode {
    CTERM_COLORS_NONE = 0,  /* attributes only, such as bold and underline */
    CTERM_COLORS_16,
    CTERM_COLORS_256,
    CTERM_COLORS_RGB        /* true colour */
} cterm_color_mode;

/* Only meaningful after cterm_init(). Use set to override the detected mode, for
 * example from a --color command-line option. */
CTERM_API cterm_color_mode cterm_get_color_mode(void);
CTERM_API void cterm_set_color_mode(cterm_color_mode mode);

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

/* printf-style text. Returns the x position just after it, like cterm_text. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 5, 6)))
#endif
CTERM_API int cterm_textf(cterm_buffer *buffer, int x, int y, cterm_style style, const char *format, ...);

/* The line styles for boxes and lines. Terminal fonts almost always have all of
 * these; CTERM_BORDER_ASCII is there for those that don't. */
typedef enum cterm_border {
    CTERM_BORDER_SINGLE = 0,  /* corners and lines like: ┌─┐ */
    CTERM_BORDER_DOUBLE,      /* ╔═╗ */
    CTERM_BORDER_ROUNDED,     /* ╭─╮ */
    CTERM_BORDER_HEAVY,       /* ┏━┓ */
    CTERM_BORDER_ASCII        /* +-+ */
} cterm_border;

/* Draws the outline of a rectangle, leaving the inside as it was. To draw inside
 * the box, use cterm_rect_inset(area, 1). */
CTERM_API void cterm_box(cterm_buffer *buffer, cterm_rect area, cterm_border border, cterm_style style);

/* Draws a straight line, `length` cells long, going right or down from (x, y). */
CTERM_API void cterm_hline(cterm_buffer *buffer, int x, int y, int length, cterm_border border, cterm_style style);
CTERM_API void cterm_vline(cterm_buffer *buffer, int x, int y, int length, cterm_border border, cterm_style style);

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

/* --- Events -------------------------------------------------------------- */

typedef enum cterm_event_type {
    CTERM_EVENT_NONE = 0,
    CTERM_EVENT_KEY,     /* a key press: see `key` and `mods` */
    CTERM_EVENT_MOUSE,   /* see `action`, `button`, `x`, `y` and `mods` */
    CTERM_EVENT_RESIZE,  /* the terminal changed size, or the app came back after
                            being suspended: see `width` and `height`. Draw
                            everything again. */
    CTERM_EVENT_PASTE    /* text was pasted: see `text` and `text_length` */
} cterm_event_type;

/* Values of event.key for keys that aren't text. Text keys hold their Unicode
 * code point instead, for example 'a' or 0x995. These all sit above the largest
 * code point, so the two can never be confused. */
enum {
    CTERM_KEY_ENTER = 0x110000,
    CTERM_KEY_TAB,        /* Shift+Tab is CTERM_KEY_TAB with CTERM_MOD_SHIFT */
    CTERM_KEY_BACKSPACE,
    CTERM_KEY_ESC,
    CTERM_KEY_UP,
    CTERM_KEY_DOWN,
    CTERM_KEY_LEFT,
    CTERM_KEY_RIGHT,
    CTERM_KEY_HOME,
    CTERM_KEY_END,
    CTERM_KEY_PAGE_UP,
    CTERM_KEY_PAGE_DOWN,
    CTERM_KEY_INSERT,
    CTERM_KEY_DELETE,
    CTERM_KEY_F1, CTERM_KEY_F2, CTERM_KEY_F3, CTERM_KEY_F4,
    CTERM_KEY_F5, CTERM_KEY_F6, CTERM_KEY_F7, CTERM_KEY_F8,
    CTERM_KEY_F9, CTERM_KEY_F10, CTERM_KEY_F11, CTERM_KEY_F12
};

/* Values of event.mods. Combine them with &. Ctrl+C arrives as key 'c' with
 * CTERM_MOD_CTRL: it doesn't stop the program, so your app decides what it does. */
enum {
    CTERM_MOD_SHIFT = 1 << 0,
    CTERM_MOD_ALT   = 1 << 1,
    CTERM_MOD_CTRL  = 1 << 2
};

/* Values of event.action. */
typedef enum cterm_mouse_action {
    CTERM_MOUSE_PRESS = 1,
    CTERM_MOUSE_RELEASE,
    CTERM_MOUSE_DRAG,        /* moved with a button held */
    CTERM_MOUSE_MOVE,        /* moved with no button held (needs CTERM_MOUSE_MOTION) */
    CTERM_MOUSE_SCROLL_UP,
    CTERM_MOUSE_SCROLL_DOWN,
    CTERM_MOUSE_SCROLL_LEFT,
    CTERM_MOUSE_SCROLL_RIGHT
} cterm_mouse_action;

/* Values of event.button. */
typedef enum cterm_mouse_button {
    CTERM_BUTTON_NONE = 0,
    CTERM_BUTTON_LEFT,
    CTERM_BUTTON_MIDDLE,
    CTERM_BUTTON_RIGHT
} cterm_mouse_button;

/* One input event. Check `type`, then read the fields listed for that type. */
typedef struct cterm_event {
    cterm_event_type type;
    int key;                   /* KEY: a code point, or a CTERM_KEY_ value */
    int mods;                  /* KEY, MOUSE: CTERM_MOD_ values */
    int x, y;                  /* MOUSE: the cell under the pointer */
    cterm_mouse_action action; /* MOUSE */
    cterm_mouse_button button; /* MOUSE */
    int width, height;         /* RESIZE */
    const char *text;          /* PASTE: NUL-terminated and may be invalid UTF-8.
                                  It's only valid until the next cterm_poll(). */
    size_t text_length;        /* PASTE: length of text in bytes */
} cterm_event;

/* Waits for the next event.
 *   timeout_ms < 0    wait for as long as it takes
 *   timeout_ms == 0   don't wait
 *   timeout_ms > 0    wait up to that many milliseconds
 *
 * Returns 1 and fills in *event if there was one, 0 if the time ran out, or -1
 * if the terminal has gone away. Your loop should stop on -1. */
CTERM_API int cterm_poll(cterm_event *event, int timeout_ms);

/* Mouse reporting is off by default, because turning it on stops the user from
 * selecting text with the mouse in most terminals. */
typedef enum cterm_mouse_mode {
    CTERM_MOUSE_OFF = 0,
    CTERM_MOUSE_BUTTONS,  /* presses, releases, drags and the scroll wheel */
    CTERM_MOUSE_MOTION    /* all of that, plus movement with no button held */
} cterm_mouse_mode;

CTERM_API void cterm_set_mouse(cterm_mouse_mode mode);

/* The UI layer below is optional. Define CTERM_NO_UI to leave it out. */
#ifndef CTERM_NO_UI

/* --- Layout -------------------------------------------------------------- */

/* Sizes for cterm_split_rows and cterm_split_columns:
 *
 *   CTERM_FIXED(n)     exactly n cells
 *   CTERM_PERCENT(p)   p percent of the space
 *   CTERM_FILL(w)      a share of whatever is left over, in proportion to w. Two
 *                      CTERM_FILL(1) get half each; CTERM_FILL(2) gets twice the
 *                      space of CTERM_FILL(1).
 *
 * If there's not enough room, children near the end get less, down to nothing. */
#define CTERM_FIXED(n)    (n)
#define CTERM_PERCENT(p)  (1000000 + (p))
#define CTERM_FILL(w)     (-(w))

/* Splits `area` into `count` rows or columns, stored in `out`, with `gap` empty
 * cells between them. Nest calls to build any layout:
 *
 *     int rows[] = { CTERM_FIXED(1), CTERM_FILL(1), CTERM_FIXED(1) };
 *     cterm_rect row[3];
 *     cterm_split_rows(cterm_bounds(screen), 0, rows, 3, row);
 *     // row[0] is a header, row[1] the main area, and row[2] a footer. */
CTERM_API void cterm_split_rows(cterm_rect area, int gap, const int *sizes, int count, cterm_rect *out);
CTERM_API void cterm_split_columns(cterm_rect area, int gap, const int *sizes, int count, cterm_rect *out);

/* --- Widgets ------------------------------------------------------------- */

/* The widgets are "immediate mode": you don't create and manage widget objects.
 * Each frame you call a function that draws the widget and handles the current
 * event, and it tells you what happened:
 *
 *     cterm_ui ui;
 *     cterm_ui_init(&ui);
 *     ...
 *     cterm_clear(screen);
 *     cterm_ui_begin(&ui, screen, &event);
 *     if (cterm_ui_input(&ui, area, &name) & CTERM_UI_SUBMITTED) save(name.text);
 *     cterm_ui_end(&ui);
 *     cterm_present();
 *
 * Your app owns the state (cterm_input, cterm_list, cterm_scroll). A widget is
 * identified by the address of its state, so that state must stay in the same
 * place between frames: make it static, or part of a long-lived struct. */

/* What a widget call can return. Combine with &. */
enum {
    CTERM_UI_CHANGED   = 1 << 0, /* the text or selection changed */
    CTERM_UI_SUBMITTED = 1 << 1  /* the user pressed Enter */
};

/* The styles widgets draw with. cterm_ui_init sets defaults that work even with no
 * colour. Change any of them in ui.theme whenever you like. */
typedef struct cterm_ui_theme {
    cterm_style input;              /* a text input */
    cterm_style input_focused;
    cterm_style placeholder;        /* hint text in an empty input */
    cterm_style list_item;
    cterm_style list_selected;      /* the selected row, in a focused list */
    cterm_style list_selected_idle; /* the selected row, in an unfocused list */
    cterm_style scrollbar_track;
    cterm_style scrollbar_thumb;
    cterm_style scrollbar_thumb_focused;
} cterm_ui_theme;

/* The largest number of widgets Tab can reach and clicks can focus. Extra widgets
 * still draw, but can't be focused. */
#ifndef CTERM_UI_MAX_WIDGETS
  #define CTERM_UI_MAX_WIDGETS 128
#endif

typedef struct cterm_ui_slot {
    const void *id;
    cterm_rect area;
} cterm_ui_slot;

/* Everything the UI remembers between frames. Create one with cterm_ui_init and
 * use it for the life of the app. Only `theme` is meant to be touched directly. */
typedef struct cterm_ui {
    cterm_ui_theme theme;

    /* Private. */
    cterm_buffer *buffer;
    const cterm_event *event;
    int event_used;
    const void *focus;
    int focus_seen;
    const void *mouse_capture;
    cterm_ui_slot widgets[2][CTERM_UI_MAX_WIDGETS]; /* last frame's and this frame's */
    int widget_count[2];
    int current;                                    /* which of the two is this frame's */
} cterm_ui;

CTERM_API void cterm_ui_init(cterm_ui *ui);

/* Starts a frame that draws on `buffer` and handles `event`. Pass the event from
 * cterm_poll, or NULL if there's none. Tab and Shift+Tab move the focus here. */
CTERM_API void cterm_ui_begin(cterm_ui *ui, cterm_buffer *buffer, const cterm_event *event);
CTERM_API void cterm_ui_end(cterm_ui *ui);

/* 1 if a widget used this frame's event. Check it after your widgets to find out
 * whether a key is still yours to handle. */
CTERM_API int cterm_ui_event_handled(const cterm_ui *ui);

/* Moves the focus to the widget with this state, or asks about it. The first
 * focusable widget drawn gets the focus if nothing has it. */
CTERM_API void cterm_ui_focus(cterm_ui *ui, const void *widget_state);
CTERM_API int cterm_ui_has_focus(const cterm_ui *ui, const void *widget_state);

/* --- Text input ---------------------------------------------------------- */

/* A one-line text field. `text` is an ordinary NUL-terminated UTF-8 string that you
 * can read at any time. */
typedef struct cterm_input {
    char *text;              /* your storage */
    size_t capacity;         /* its size in bytes, including the NUL */
    size_t cursor;           /* the byte offset of the cursor in text */
    int scroll;              /* cells hidden off the left edge */
    const char *placeholder; /* optional hint shown while the field is empty */
    uint32_t mask;           /* optional: draw this character instead of the text,
                                for example '*' for a password */
} cterm_input;

/* Sets up an input using `storage`, which it empties. */
CTERM_API void cterm_input_init(cterm_input *input, char *storage, size_t capacity);

/* Replaces the text, cutting it short if it doesn't fit, and moves the cursor to the
 * end. Control characters are removed, as they are for typing and pasting. */
CTERM_API void cterm_input_set(cterm_input *input, const char *utf8);

/* Draws the input on the top row of `area` and handles the event. Returns
 * CTERM_UI_ flags.
 *
 * Keys: arrows, Home/End, Backspace, Delete, Ctrl+A/E (start/end), Ctrl+Left/Right
 * and Alt+B/F (by word), Ctrl+W and Alt+Backspace (delete word), Ctrl+U/K (delete
 * to start/end), and Enter (submit). */
CTERM_API int cterm_ui_input(cterm_ui *ui, cterm_rect area, cterm_input *input);

/* --- List ---------------------------------------------------------------- */

/* A scrolling list with one selected row. A struct of zeros is a valid start. You
 * can set `selected` yourself: the list scrolls to show it. */
typedef struct cterm_list {
    int selected;
    int scroll;           /* the first visible row */
    int last_selected;    /* private */
    int last_height;      /* private */
} cterm_list;

/* Draws a list of strings. Returns CTERM_UI_ flags: CHANGED means the selection
 * moved, and SUBMITTED means Enter was pressed on the selected row.
 *
 * Keys: Up/Down, PageUp/PageDown, Home/End, Enter. Clicking selects a row, and the
 * mouse wheel scrolls. */
CTERM_API int cterm_ui_list(cterm_ui *ui, cterm_rect area, cterm_list *list, const char *const *items, int count);

/* The same, but it asks for each row's text when it's needed, so a huge or computed
 * list doesn't have to exist in memory. The returned string only has to stay valid
 * until the next call. It's only called for visible rows. */
typedef const char *(*cterm_list_text_fn)(int index, void *user_data);
CTERM_API int cterm_ui_list_fn(cterm_ui *ui, cterm_rect area, cterm_list *list, int count,
                               cterm_list_text_fn text, void *user_data);

/* --- Scrollable view ----------------------------------------------------- */

/* A window onto content larger than the space available. A struct of zeros is a
 * valid start. */
typedef struct cterm_scroll {
    int x, y;                  /* how far it's scrolled */

    /* Private. */
    cterm_rect viewport;
    cterm_rect saved_clip;
    int content_height;
    int bar_visible;
} cterm_scroll;

/* Starts a scrollable area. Pass the full size of your content; a content_width of
 * 0 means "as wide as the view". Draw between begin and end, positioned relative to
 * the returned rectangle, as if it were all visible: cterm clips what isn't.
 *
 *     cterm_rect c = cterm_ui_scroll_begin(&ui, area, &scroll, 0, line_count);
 *     for (i = 0; i < line_count; i++) cterm_text(screen, c.x, c.y + i, style, lines[i]);
 *     cterm_ui_scroll_end(&ui, &scroll);
 *
 * Keys: arrows, PageUp/PageDown, Home/End. The mouse wheel and scrollbar also work.
 * Other widgets can go inside, but the wheel always scrolls the outer view. */
CTERM_API cterm_rect cterm_ui_scroll_begin(cterm_ui *ui, cterm_rect area, cterm_scroll *scroll,
                                           int content_width, int content_height);
CTERM_API void cterm_ui_scroll_end(cterm_ui *ui, cterm_scroll *scroll);

#endif /* CTERM_NO_UI */

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

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
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
  /* SA_RESTART is missing if libc has already been set up to hide POSIX. */
  #ifndef SA_RESTART
    #error "cterm.h: #define CTERM_IMPLEMENTATION and #include \"cterm.h\" must come before every other #include in this file (or compile with -std=gnu99 instead of -std=c99)"
  #endif
  #include <errno.h>
  #include <fcntl.h>
  #include <sys/ioctl.h>
  #include <sys/select.h>
  #include <termios.h>
  #include <time.h>
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

CTERM_API int cterm_textf(cterm_buffer *buffer, int x, int y, cterm_style style, const char *format, ...) {
    char small[256];
    va_list args;
    int length;

    /* Most text fits on the stack. vsnprintf reports the size it really needed, so
     * longer text gets a second attempt with a big enough allocation. */
    va_start(args, format);
    length = vsnprintf(small, sizeof small, format, args);
    va_end(args);
    if (length < 0) return x;
    if ((size_t)length < sizeof small) return cterm_textn(buffer, x, y, style, small, (size_t)length);

    {
        char *big = (char *)CTERM_MALLOC((size_t)length + 1);
        if (!big) {
            /* Show what fits rather than nothing. */
            return cterm_textn(buffer, x, y, style, small, sizeof small - 1);
        }
        va_start(args, format);
        vsnprintf(big, (size_t)length + 1, format, args);
        va_end(args);
        x = cterm_textn(buffer, x, y, style, big, (size_t)length);
        CTERM_FREE(big);
        return x;
    }
}

/* The characters for each cterm_border. */
static const struct {
    uint32_t horizontal, vertical, top_left, top_right, bottom_left, bottom_right;
} cterm__borders[] = {
    { 0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518 }, /* single  */
    { 0x2550, 0x2551, 0x2554, 0x2557, 0x255A, 0x255D }, /* double  */
    { 0x2500, 0x2502, 0x256D, 0x256E, 0x2570, 0x256F }, /* rounded */
    { 0x2501, 0x2503, 0x250F, 0x2513, 0x2517, 0x251B }, /* heavy   */
    { '-',    '|',    '+',    '+',    '+',    '+'    }  /* ASCII   */
};

CTERM_API void cterm_hline(cterm_buffer *buffer, int x, int y, int length, cterm_border border, cterm_style style) {
    cterm_fill(buffer, cterm_rect_make(x, y, length, 1), cterm__borders[border].horizontal, style);
}

CTERM_API void cterm_vline(cterm_buffer *buffer, int x, int y, int length, cterm_border border, cterm_style style) {
    cterm_fill(buffer, cterm_rect_make(x, y, 1, length), cterm__borders[border].vertical, style);
}

CTERM_API void cterm_box(cterm_buffer *buffer, cterm_rect area, cterm_border border, cterm_style style) {
    int right = area.x + area.w - 1, bottom = area.y + area.h - 1;
    if (area.w <= 0 || area.h <= 0) return;

    /* Too thin to have corners, so it's just a line. */
    if (area.h == 1) {
        cterm_hline(buffer, area.x, area.y, area.w, border, style);
        return;
    }
    if (area.w == 1) {
        cterm_vline(buffer, area.x, area.y, area.h, border, style);
        return;
    }

    cterm_hline(buffer, area.x + 1, area.y, area.w - 2, border, style);
    cterm_hline(buffer, area.x + 1, bottom, area.w - 2, border, style);
    cterm_vline(buffer, area.x, area.y + 1, area.h - 2, border, style);
    cterm_vline(buffer, right, area.y + 1, area.h - 2, border, style);
    cterm_put(buffer, area.x, area.y, cterm__borders[border].top_left, style);
    cterm_put(buffer, right, area.y, cterm__borders[border].top_right, style);
    cterm_put(buffer, area.x, bottom, cterm__borders[border].bottom_left, style);
    cterm_put(buffer, right, bottom, cterm__borders[border].bottom_right, style);
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

/* --- Colour fallback ----------------------------------------------------- */

/* The usual xterm values of the 16 basic colours. Terminals let users change these,
 * so they're only a best guess, but that's fine for choosing a nearest match. */
static const unsigned char cterm__basic_rgb[16][3] = {
    {0, 0, 0}, {205, 0, 0}, {0, 205, 0}, {205, 205, 0}, {0, 0, 238}, {205, 0, 205}, {0, 205, 205}, {229, 229, 229},
    {127, 127, 127}, {255, 0, 0}, {0, 255, 0}, {255, 255, 0}, {92, 92, 255}, {255, 0, 255}, {0, 255, 255}, {255, 255, 255}
};

/* Colours 16-231 form a 6x6x6 cube using these levels for each of red, green, blue. */
static const unsigned char cterm__cube_levels[6] = { 0, 95, 135, 175, 215, 255 };

static int cterm__color_distance(int r1, int g1, int b1, int r2, int g2, int b2) {
    return (r1 - r2) * (r1 - r2) + (g1 - g2) * (g1 - g2) + (b1 - b2) * (b1 - b2);
}

/* The nearest palette colour above 15, trying both the cube and the grey ramp. */
static int cterm__rgb_to_256(int r, int g, int b) {
    int ri = 0, gi = 0, bi = 0, i, grey_index, grey_level, cube_distance, grey_distance;

    for (i = 0; i < 6; i++) {
        if (abs(cterm__cube_levels[i] - r) < abs(cterm__cube_levels[ri] - r)) ri = i;
        if (abs(cterm__cube_levels[i] - g) < abs(cterm__cube_levels[gi] - g)) gi = i;
        if (abs(cterm__cube_levels[i] - b) < abs(cterm__cube_levels[bi] - b)) bi = i;
    }
    cube_distance = cterm__color_distance(r, g, b, cterm__cube_levels[ri], cterm__cube_levels[gi], cterm__cube_levels[bi]);

    /* Colours 232-255 are greys: 8, 18, 28, ... 238. */
    grey_index = ((r + g + b) / 3 - 3) / 10;
    if (grey_index < 0) grey_index = 0;
    if (grey_index > 23) grey_index = 23;
    grey_level = 8 + grey_index * 10;
    grey_distance = cterm__color_distance(r, g, b, grey_level, grey_level, grey_level);

    if (grey_distance < cube_distance) return 232 + grey_index;
    return 16 + 36 * ri + 6 * gi + bi;
}

/* The RGB value of a palette colour. */
static void cterm__256_to_rgb(int index, int *r, int *g, int *b) {
    if (index < 16) {
        *r = cterm__basic_rgb[index][0];
        *g = cterm__basic_rgb[index][1];
        *b = cterm__basic_rgb[index][2];
    } else if (index < 232) {
        index -= 16;
        *r = cterm__cube_levels[index / 36];
        *g = cterm__cube_levels[(index / 6) % 6];
        *b = cterm__cube_levels[index % 6];
    } else {
        *r = *g = *b = 8 + (index - 232) * 10;
    }
}

static int cterm__rgb_to_16(int r, int g, int b) {
    int best = 0, best_distance = -1, i;
    for (i = 0; i < 16; i++) {
        int distance = cterm__color_distance(r, g, b, cterm__basic_rgb[i][0], cterm__basic_rgb[i][1], cterm__basic_rgb[i][2]);
        if (best_distance < 0 || distance < best_distance) {
            best = i;
            best_distance = distance;
        }
    }
    return best;
}

/* Converts a colour to the nearest one a terminal in this mode can show. */
static cterm_color cterm__downgrade(cterm_color color, cterm_color_mode mode) {
    int is_rgb = (color & 0xFF000000u) == 0x02000000u;
    int r, g, b;

    if (color == CTERM_DEFAULT || mode == CTERM_COLORS_RGB) return color;
    if (mode == CTERM_COLORS_NONE) return CTERM_DEFAULT;

    if (is_rgb) {
        r = (int)((color >> 16) & 0xFFu);
        g = (int)((color >> 8) & 0xFFu);
        b = (int)(color & 0xFFu);
        if (mode == CTERM_COLORS_256) return CTERM_COLOR256(cterm__rgb_to_256(r, g, b));
        return CTERM_COLOR256(cterm__rgb_to_16(r, g, b));
    }

    /* A palette colour in 16-colour mode. Below 16, there's nothing to do. */
    if (mode == CTERM_COLORS_16 && (color & 0xFFu) >= 16) {
        cterm__256_to_rgb((int)(color & 0xFFu), &r, &g, &b);
        return CTERM_COLOR256(cterm__rgb_to_16(r, g, b));
    }
    return color;
}

/* Works out the colour support from environment variables (NULL if unset). It's
 * separate from getenv() so tests can try every combination. */
static cterm_color_mode cterm__color_mode_from_env(const char *no_color, const char *colorterm, const char *term) {
    /* no-color.org: any non-empty value of NO_COLOR means no colour. */
    if (no_color && no_color[0]) return CTERM_COLORS_NONE;
    if (term && strcmp(term, "dumb") == 0) return CTERM_COLORS_NONE;
    if (colorterm && (strcmp(colorterm, "truecolor") == 0 || strcmp(colorterm, "24bit") == 0)) return CTERM_COLORS_RGB;
    if (term && (strstr(term, "truecolor") || strstr(term, "24bit") || strstr(term, "direct"))) return CTERM_COLORS_RGB;
    if (term && strstr(term, "256")) return CTERM_COLORS_256;
    return CTERM_COLORS_16;
}

/* Appends one colour to an SGR ("set style") sequence. The basic 16 use the original
 * short codes, which are the only ones the oldest terminals understand. */
static void cterm__sgr_color(cterm__sb *out, cterm_color color, int background) {
    uint32_t kind = color & 0xFF000000u;
    if (color == CTERM_DEFAULT) return; /* the reset at the start of the sequence covers it */
    if (kind == 0x01000000u) {
        int index = (int)(color & 0xFFu);
        cterm__sb_str(out, ";");
        if (index < 8) {
            cterm__sb_int(out, (background ? 40 : 30) + index);
        } else if (index < 16) {
            cterm__sb_int(out, (background ? 100 : 90) + index - 8);
        } else {
            cterm__sb_str(out, background ? "48;5;" : "38;5;");
            cterm__sb_int(out, index);
        }
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
static void cterm__sgr_style(cterm__sb *out, cterm_style style, cterm_color_mode mode) {
    cterm__sb_str(out, "\x1b[0");
    if (style.attrs & CTERM_BOLD)      cterm__sb_str(out, ";1");
    if (style.attrs & CTERM_DIM)       cterm__sb_str(out, ";2");
    if (style.attrs & CTERM_ITALIC)    cterm__sb_str(out, ";3");
    if (style.attrs & CTERM_UNDERLINE) cterm__sb_str(out, ";4");
    if (style.attrs & CTERM_BLINK)     cterm__sb_str(out, ";5");
    if (style.attrs & CTERM_REVERSE)   cterm__sb_str(out, ";7");
    if (style.attrs & CTERM_STRIKE)    cterm__sb_str(out, ";9");
    cterm__sgr_color(out, cterm__downgrade(style.fg, mode), 0);
    cterm__sgr_color(out, cterm__downgrade(style.bg, mode), 1);
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

/* Terminals don't all agree how wide some characters are: wide characters and emoji,
 * text with marks, and the scripts from Hebrew through to Southeast Asian (U+0590 to
 * U+1FFF), whose vowel signs some terminals count as zero cells and others as one.
 * After drawing one, the renderer forgets where the cursor is, which forces an explicit
 * move next, so a disagreement can't push the rest of the row sideways. It costs a
 * few bytes per character, which is why common ranges like box drawing are left out. */
static int cterm__width_is_disputed(uint32_t cp) {
    return cp >= 0x1F000 || (cp >= 0x0590 && cp <= 0x1FFF);
}

/* Appends what's needed to make the terminal match `back`, and updates `front`
 * to match as well. `front` and `back` must be the same size.
 *
 * With `full` set, the screen is cleared first and everything is repainted,
 * which recovers from a resize or from anything else writing to the screen. */
static void cterm__render(cterm__sb *out, cterm_buffer *front, const cterm_buffer *back,
                          cterm__render_state *state, cterm_color_mode mode, int full) {
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
                cterm__sgr_style(out, want->style, mode);
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

                if (width == 2 || want->marks[0] || cterm__width_is_disputed(want->ch)) {
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
 * INPUT PARSER
 *
 * Turns the byte stream from the terminal into events. Terminals send plain keys
 * as themselves and everything else as an "escape sequence":
 *
 *     a           the key a
 *     ESC [ A     the Up arrow
 *     ESC [ 1 ; 5 A           Ctrl+Up
 *     ESC [ < 0 ; 10 ; 5 M    left mouse press at column 10, row 5
 *
 * Two awkward facts shape the design:
 *
 *  1. Bytes arrive in arbitrary pieces, so a sequence can be cut in half. The
 *     parser keeps unused bytes until the rest arrive.
 *
 *  2. The Esc key sends a single ESC byte, which is also how every sequence
 *     starts. The only way to tell them apart is time: if nothing follows quickly,
 *     it was the Esc key. The parser doesn't know about time. Instead its caller
 *     passes `flush` = 1 once enough time has gone by, meaning "nothing more is
 *     coming, so make the best of what you have".
 *
 * Like the renderer, this never touches a terminal, so tests can feed it bytes
 * directly (see tests/test_parser.c).
 * ========================================================================= */

/* How long to wait after a lone ESC byte before deciding it was the Esc key. Too
 * short and arrow keys can break over slow connections; too long and Esc feels
 * laggy. */
#ifndef CTERM_ESC_TIMEOUT_MS
  #define CTERM_ESC_TIMEOUT_MS 50
#endif

/* How long a paste may stall before cterm gives up waiting for its end marker. A
 * paste that never ends would otherwise swallow all further input. */
#ifndef CTERM_PASTE_TIMEOUT_MS
  #define CTERM_PASTE_TIMEOUT_MS 500
#endif

/* The longest paste kept, in bytes. Anything past this is dropped. */
#ifndef CTERM_PASTE_MAX
  #define CTERM_PASTE_MAX (16u * 1024u * 1024u)
#endif

#define CTERM__ESC 0x1B
#define CTERM__CSI_MAX 64          /* longer "sequences" are treated as garbage */
#define CTERM__PASTE_END "\x1b[201~"
#define CTERM__PASTE_END_LEN 6
#define CTERM__PASTE_BEGIN_SIGNAL (-1) /* internal: stored in event.key */

typedef struct cterm__parser {
    unsigned char *data;      /* bytes received but not yet decoded */
    size_t head, tail, cap;   /* data[head..tail) is the unread part */
    int in_paste;
    char *paste;              /* the paste being collected */
    size_t paste_len, paste_cap;
} cterm__parser;

static void cterm__parser_free(cterm__parser *p) {
    CTERM_FREE(p->data);
    CTERM_FREE(p->paste);
    memset(p, 0, sizeof *p);
}

static void cterm__parser_feed(cterm__parser *p, const unsigned char *bytes, size_t n) {
    /* Slide unread bytes to the front, so the buffer doesn't grow forever. */
    if (p->head > 0) {
        size_t unread = p->tail - p->head;
        if (unread > 0) memmove(p->data, p->data + p->head, unread);
        p->head = 0;
        p->tail = unread;
    }
    if (p->tail + n > p->cap) {
        size_t cap = p->cap ? p->cap : 1024;
        unsigned char *data;
        while (cap < p->tail + n) cap *= 2;
        data = (unsigned char *)CTERM_REALLOC(p->data, cap);
        if (!data) return; /* out of memory: losing input is the only option */
        p->data = data;
        p->cap = cap;
    }
    if (n > 0) memcpy(p->data + p->tail, bytes, n);
    p->tail += n;
}

/* True if there are bytes waiting to be completed. */
static int cterm__parser_pending(const cterm__parser *p) {
    return p->in_paste || p->tail > p->head;
}

static size_t cterm__set_key(cterm_event *ev, int key, int mods, size_t used) {
    ev->type = CTERM_EVENT_KEY;
    ev->key = key;
    ev->mods = mods;
    return used;
}

/* Sequences encode modifiers as 1 + bits: 1 Shift, 2 Alt, 4 Ctrl (8 Super, which
 * is ignored). For example 5 means Ctrl and 6 means Ctrl+Shift. */
static int cterm__decode_mods(int value) {
    int bits = value - 1, mods = 0;
    if (value < 2) return 0;
    if (bits & 1) mods |= CTERM_MOD_SHIFT;
    if (bits & 2) mods |= CTERM_MOD_ALT;
    if (bits & 4) mods |= CTERM_MOD_CTRL;
    return mods;
}

/* Maps the bytes that terminals send for Enter, Tab and Ctrl+letter. */
static size_t cterm__control_key(int c, cterm_event *ev, size_t used) {
    switch (c) {
        case 0x0D: return cterm__set_key(ev, CTERM_KEY_ENTER, 0, used);
        case 0x09: return cterm__set_key(ev, CTERM_KEY_TAB, 0, used);
        /* Terminals disagree about which byte Backspace sends. Ctrl+H shares 0x08,
         * but a working Backspace matters much more than Ctrl+H. */
        case 0x7F:
        case 0x08: return cterm__set_key(ev, CTERM_KEY_BACKSPACE, 0, used);
        case 0x00: return cterm__set_key(ev, ' ', CTERM_MOD_CTRL, used);
        default:   break;
    }
    if (c >= 0x01 && c <= 0x1A) return cterm__set_key(ev, 'a' + c - 1, CTERM_MOD_CTRL, used);
    return cterm__set_key(ev, '\\' + c - 0x1C, CTERM_MOD_CTRL, used); /* 0x1C-0x1F are Ctrl with these four */
}

/* Fills in a mouse event from the terminal's button code, which packs several
 * things together: bits 0-1 button, 2 Shift, 3 Alt, 4 Ctrl, 5 motion, 6 wheel. */
static void cterm__set_mouse(cterm_event *ev, int code, int x, int y, int released) {
    int low = code & 3;
    ev->type = CTERM_EVENT_MOUSE;
    ev->x = x > 0 ? x - 1 : 0; /* terminals count from 1, cterm from 0 */
    ev->y = y > 0 ? y - 1 : 0;
    ev->mods = 0;
    if (code & 4)  ev->mods |= CTERM_MOD_SHIFT;
    if (code & 8)  ev->mods |= CTERM_MOD_ALT;
    if (code & 16) ev->mods |= CTERM_MOD_CTRL;
    ev->button = low == 0 ? CTERM_BUTTON_LEFT : low == 1 ? CTERM_BUTTON_MIDDLE :
                 low == 2 ? CTERM_BUTTON_RIGHT : CTERM_BUTTON_NONE;
    if (code & 64) {
        static const cterm_mouse_action wheel[4] = {
            CTERM_MOUSE_SCROLL_UP, CTERM_MOUSE_SCROLL_DOWN, CTERM_MOUSE_SCROLL_LEFT, CTERM_MOUSE_SCROLL_RIGHT
        };
        ev->action = wheel[low];
        ev->button = CTERM_BUTTON_NONE;
    } else if (code & 32) {
        ev->action = low == 3 ? CTERM_MOUSE_MOVE : CTERM_MOUSE_DRAG;
    } else if (released) {
        ev->action = CTERM_MOUSE_RELEASE;
    } else {
        ev->action = CTERM_MOUSE_PRESS;
    }
}

/* Handles the sequence form that ends in a `~`: ESC [ number ; mods ~ */
static void cterm__csi_tilde(const int *params, int count, cterm_event *ev) {
    int mods = count >= 2 ? cterm__decode_mods(params[1]) : 0;
    int key;
    switch (count >= 1 ? params[0] : 0) {
        case 1: case 7: key = CTERM_KEY_HOME; break;
        case 2:  key = CTERM_KEY_INSERT; break;
        case 3:  key = CTERM_KEY_DELETE; break;
        case 4: case 8: key = CTERM_KEY_END; break;
        case 5:  key = CTERM_KEY_PAGE_UP; break;
        case 6:  key = CTERM_KEY_PAGE_DOWN; break;
        case 11: key = CTERM_KEY_F1; break;
        case 12: key = CTERM_KEY_F2; break;
        case 13: key = CTERM_KEY_F3; break;
        case 14: key = CTERM_KEY_F4; break;
        case 15: key = CTERM_KEY_F5; break;
        case 17: key = CTERM_KEY_F6; break;
        case 18: key = CTERM_KEY_F7; break;
        case 19: key = CTERM_KEY_F8; break;
        case 20: key = CTERM_KEY_F9; break;
        case 21: key = CTERM_KEY_F10; break;
        case 23: key = CTERM_KEY_F11; break;
        case 24: key = CTERM_KEY_F12; break;
        case 200:
            /* The start of a paste. Tell the caller without producing an event. */
            ev->key = CTERM__PASTE_BEGIN_SIGNAL;
            return;
        case 27:
            /* xterm's "modifyOtherKeys": ESC [ 27 ; mods ; code ~ */
            if (count >= 3) {
                int code = params[2];
                if (code == 13) key = CTERM_KEY_ENTER;
                else if (code == 9) key = CTERM_KEY_TAB;
                else if (code == 27) key = CTERM_KEY_ESC;
                else if (code == 127 || code == 8) key = CTERM_KEY_BACKSPACE;
                else if (code >= 0x20 && code <= 0x10FFFF) key = code;
                else return;
                cterm__set_key(ev, key, mods, 0);
            }
            return;
        default:
            return; /* a key we don't know: ignore it */
    }
    cterm__set_key(ev, key, mods, 0);
}

/* Handles sequences that end in a letter: ESC [ 1 ; mods A, and the "CSI u" form
 * some newer terminals use for keys that older protocols couldn't express. */
static void cterm__csi_letter(int final, const int *params, int count, cterm_event *ev) {
    int mods = count >= 2 ? cterm__decode_mods(params[1]) : 0;
    int key;
    switch (final) {
        case 'A': key = CTERM_KEY_UP; break;
        case 'B': key = CTERM_KEY_DOWN; break;
        case 'C': key = CTERM_KEY_RIGHT; break;
        case 'D': key = CTERM_KEY_LEFT; break;
        case 'H': key = CTERM_KEY_HOME; break;
        case 'F': key = CTERM_KEY_END; break;
        case 'P': key = CTERM_KEY_F1; break;
        case 'Q': key = CTERM_KEY_F2; break;
        case 'R': key = CTERM_KEY_F3; break;
        case 'S': key = CTERM_KEY_F4; break;
        case 'Z': key = CTERM_KEY_TAB; mods |= CTERM_MOD_SHIFT; break;
        case 'u':
            if (count < 1) return;
            if (params[0] == 13) key = CTERM_KEY_ENTER;
            else if (params[0] == 9) key = CTERM_KEY_TAB;
            else if (params[0] == 27) key = CTERM_KEY_ESC;
            else if (params[0] == 127 || params[0] == 8) key = CTERM_KEY_BACKSPACE;
            else if (params[0] >= 0x20 && params[0] <= 0x10FFFF) key = params[0];
            else return;
            break;
        default:
            return; /* focus reports and so on: not used */
    }
    cterm__set_key(ev, key, mods, 0);
}

/* Parses ESC [ ... Returns the bytes used, or 0 if more are needed. */
static size_t cterm__parse_csi(const unsigned char *s, size_t n, cterm_event *ev, int flush) {
    int params[8], count = 0, value = 0, in_value = 0, marker = 0, odd = 0;
    size_t i, final_at;

    if (n == 2) {
        return flush ? cterm__set_key(ev, '[', CTERM_MOD_ALT, 2) : 0; /* Alt+[ */
    }

    /* The Linux console's F1-F5: ESC [ [ A */
    if (s[2] == '[') {
        if (n < 4) return flush ? n : 0;
        if (s[3] >= 'A' && s[3] <= 'E') cterm__set_key(ev, CTERM_KEY_F1 + (s[3] - 'A'), 0, 0);
        return 4;
    }

    /* Old-style mouse report: ESC [ M followed by three raw bytes. It's only seen
     * on terminals that ignore the modern SGR format cterm asks for. */
    if (s[2] == 'M') {
        int code;
        if (n < 6) return flush ? n : 0;
        code = s[3] >= 32 ? s[3] - 32 : 0;
        /* This format has no release flag: button number 3 means "released". */
        cterm__set_mouse(ev, code, s[4] >= 32 ? s[4] - 32 : 0, s[5] >= 32 ? s[5] - 32 : 0,
                         (code & 3) == 3 && !(code & (32 | 64)));
        return 6;
    }

    /* Find the final byte. A valid sequence only has 0x20-0x3F before its final
     * byte. Anything else means the sequence got cut off, so drop what we have
     * and let the next call handle that byte normally. */
    for (i = 2; i < n; i++) {
        unsigned char c = s[i];
        if (c >= 0x40 && c <= 0x7E) break;
        if (c < 0x20 || c > 0x7E) return i;
        if (i >= CTERM__CSI_MAX) return i;
    }
    if (i == n) return flush ? n : 0;
    final_at = i;

    /* Read the numbers. An optional marker character comes first, and numbers are
     * separated by `;`. A `:` introduces extra detail that cterm doesn't need. */
    i = 2;
    if (s[i] >= 0x3C && s[i] <= 0x3F) marker = s[i++];
    for (; i < final_at; i++) {
        unsigned char c = s[i];
        if (c >= '0' && c <= '9') {
            if (value < 100000) value = value * 10 + (c - '0');
            in_value = 1;
        } else if (c == ';' || c == ':') {
            if (count < 8) params[count++] = value;
            value = 0;
            in_value = 0;
            if (c == ':') {
                /* Skip the detail, including the `;` that ends it. */
                while (i + 1 < final_at && s[i + 1] != ';') i++;
                if (i + 1 < final_at) i++;
            }
        } else {
            odd = 1; /* an intermediate byte: not a sequence we use */
        }
    }
    if ((in_value || count > 0) && count < 8) params[count++] = value;

    if (!odd) {
        int final = s[final_at];
        if (marker == '<') {
            if ((final == 'M' || final == 'm') && count >= 3) {
                cterm__set_mouse(ev, params[0], params[1], params[2], final == 'm');
            }
        } else if (marker == 0) {
            if (final == '~') cterm__csi_tilde(params, count, ev);
            else cterm__csi_letter(final, params, count, ev);
        }
    }
    return final_at + 1;
}

/* Parses ESC O ..., used by F1-F4, and by arrows in "application mode". */
static size_t cterm__parse_ss3(const unsigned char *s, size_t n, cterm_event *ev, int flush) {
    size_t at = 2;
    int mods = 0, key;
    if (n == 2) return flush ? cterm__set_key(ev, 'O', CTERM_MOD_ALT, 2) : 0; /* Alt+O */

    /* Some terminals put a modifier digit in the middle: ESC O 5 A */
    if (s[2] >= '0' && s[2] <= '9') {
        if (n == 3) return flush ? n : 0;
        mods = cterm__decode_mods(s[2] - '0');
        at = 3;
    }
    switch (s[at]) {
        case 'A': key = CTERM_KEY_UP; break;
        case 'B': key = CTERM_KEY_DOWN; break;
        case 'C': key = CTERM_KEY_RIGHT; break;
        case 'D': key = CTERM_KEY_LEFT; break;
        case 'H': key = CTERM_KEY_HOME; break;
        case 'F': key = CTERM_KEY_END; break;
        case 'P': key = CTERM_KEY_F1; break;
        case 'Q': key = CTERM_KEY_F2; break;
        case 'R': key = CTERM_KEY_F3; break;
        case 'S': key = CTERM_KEY_F4; break;
        case 'M': key = CTERM_KEY_ENTER; break; /* keypad Enter */
        default:  return at + 1;                /* unknown: ignore */
    }
    return cterm__set_key(ev, key, mods, at + 1);
}

/* Parses one thing from the start of `s` (n >= 1).
 *
 * Returns the bytes consumed, with *ev filled in if they made an event. Bytes can
 * be consumed with no event, when they were a sequence that doesn't matter here.
 * Returns 0 if more bytes are needed, which can't happen when `flush` is set: then
 * it always consumes something, and that's what guarantees the parser never gets
 * stuck. */
static size_t cterm__parse_one(const unsigned char *s, size_t n, cterm_event *ev, int flush) {
    unsigned char c = s[0];

    if (c == CTERM__ESC) {
        size_t used;
        if (n == 1) return flush ? cterm__set_key(ev, CTERM_KEY_ESC, 0, 1) : 0;
        if (s[1] == '[') return cterm__parse_csi(s, n, ev, flush);
        if (s[1] == 'O') return cterm__parse_ss3(s, n, ev, flush);
        if (s[1] == CTERM__ESC) {
            /* ESC ESC [ A is Alt+Up on some terminals. Otherwise it's the Esc key
             * pressed twice, so give back the first and handle the second later. */
            if (n == 2) return flush ? cterm__set_key(ev, CTERM_KEY_ESC, 0, 1) : 0;
            if (s[2] != '[' && s[2] != 'O') return cterm__set_key(ev, CTERM_KEY_ESC, 0, 1);
        }
        /* ESC followed by a normal key is how terminals report Alt+key. */
        used = cterm__parse_one(s + 1, n - 1, ev, flush);
        if (used == 0) return 0;
        if (ev->type == CTERM_EVENT_KEY) ev->mods |= CTERM_MOD_ALT;
        return used + 1;
    }

    if (c < 0x20 || c == 0x7F) return cterm__control_key(c, ev, 1);

    {
        uint32_t cp = CTERM__REPLACEMENT;
        int used = cterm__utf8_decode(s, n, &cp);
        if (used == 0) {
            if (!flush) return 0;
            cp = CTERM__REPLACEMENT;
            used = 1;
        }
        return cterm__set_key(ev, (int)cp, 0, (size_t)used);
    }
}

/* Adds bytes to the paste being collected. */
static void cterm__paste_append(cterm__parser *p, const unsigned char *bytes, size_t n) {
    if (p->paste_len + n > CTERM_PASTE_MAX) {
        n = CTERM_PASTE_MAX - p->paste_len;
    }
    if (p->paste_len + n + 1 > p->paste_cap) {
        size_t cap = p->paste_cap ? p->paste_cap : 1024;
        char *data;
        while (cap < p->paste_len + n + 1) cap *= 2;
        data = (char *)CTERM_REALLOC(p->paste, cap);
        if (!data) return;
        p->paste = data;
        p->paste_cap = cap;
    }
    if (n > 0) memcpy(p->paste + p->paste_len, bytes, n);
    p->paste_len += n;
    p->paste[p->paste_len] = '\0';
}

static void cterm__paste_finish(cterm__parser *p, cterm_event *ev) {
    cterm__paste_append(p, NULL, 0); /* makes sure there's a buffer and a NUL */
    p->in_paste = 0;
    ev->type = CTERM_EVENT_PASTE;
    ev->text = p->paste ? p->paste : "";
    ev->text_length = p->paste ? p->paste_len : 0;
}

/* Collects paste bytes up to the end marker. Returns 1 when the paste is complete
 * and *ev has been filled in, or 0 if it needs more bytes. */
static int cterm__paste_step(cterm__parser *p, cterm_event *ev, int flush) {
    const unsigned char *s = p->data + p->head;
    size_t n = p->tail - p->head, i;

    for (i = 0; i + CTERM__PASTE_END_LEN <= n; i++) {
        if (s[i] == CTERM__ESC && memcmp(s + i, CTERM__PASTE_END, CTERM__PASTE_END_LEN) == 0) {
            cterm__paste_append(p, s, i);
            p->head += i + CTERM__PASTE_END_LEN;
            cterm__paste_finish(p, ev);
            return 1;
        }
    }

    if (flush) {
        cterm__paste_append(p, s, n);
        p->head = p->tail;
        cterm__paste_finish(p, ev);
        return 1;
    }

    /* The last few bytes could be the start of the marker, so they have to wait.
     * Everything before them is definitely text. */
    if (n > CTERM__PASTE_END_LEN - 1) {
        size_t safe = n - (CTERM__PASTE_END_LEN - 1);
        cterm__paste_append(p, s, safe);
        p->head += safe;
    }
    return 0;
}

/* Gets the next event from the bytes fed so far. Returns 1 and fills in *ev, or 0
 * if there isn't a complete one yet. With `flush` set, incomplete input is resolved
 * as well as it can be. */
static int cterm__parser_next(cterm__parser *p, cterm_event *ev, int flush) {
    memset(ev, 0, sizeof *ev);
    for (;;) {
        size_t n, used;
        if (p->in_paste) return cterm__paste_step(p, ev, flush);

        n = p->tail - p->head;
        if (n == 0) return 0;

        used = cterm__parse_one(p->data + p->head, n, ev, flush);
        if (used == 0) return 0;
        p->head += used;

        if (ev->type != CTERM_EVENT_NONE) return 1;
        if (ev->key == CTERM__PASTE_BEGIN_SIGNAL) {
            ev->key = 0;
            p->in_paste = 1;
            p->paste_len = 0;
        }
    }
}

/* ========================================================================= *
 * PLATFORM LAYER
 *
 * Everything that differs between operating systems lives here, behind these
 * private functions. Porting cterm to a new OS means writing this set again:
 *
 *   cterm__plat_init       enter raw mode; 0 on success, -1 via cterm__fail()
 *   cterm__plat_shutdown   undo cterm__plat_init
 *   cterm__plat_wait       wait up to timeout_ms (forever if negative) for input,
 *                          and read it as UTF-8 and escape sequences: >0 bytes,
 *                          0 nothing yet (a timeout, or a wake-up with no input),
 *                          -1 the terminal has gone
 *   cterm__plat_write      write all the bytes; 0 on success, -1 on failure
 *   cterm__plat_size       get the terminal's size in cells
 *   cterm__plat_now_ms     a clock that only moves forward, in milliseconds
 *   cterm__plat_suspend    hand the terminal back until the user resumes the app
 *
 * It sets cterm__resize_pending when the terminal changes size. It also has to make
 * sure the terminal gets restored if the program crashes.
 * ========================================================================= */

/* Entering: alternate screen (so the user's shell history comes back when we
 * finish), no auto-wrap (so drawing the bottom-right cell can't scroll the
 * screen), hidden cursor, and bracketed paste. */
#define CTERM__ENTER_SEQUENCE "\x1b[?1049h\x1b[?7l\x1b[?25l\x1b[?2004h"

/* Leaving undoes everything, including modes that may not be on, because turning
 * off something that's already off is harmless. */
#define CTERM__LEAVE_SEQUENCE \
    "\x1b[?2026l\x1b[?2004l\x1b[?1006l\x1b[?1003l\x1b[?1002l\x1b[?1000l\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l"

/* What a signal handler is allowed to touch. sig_atomic_t is the only type the C
 * standard guarantees is safe to set from one. */
static volatile sig_atomic_t cterm__resize_pending = 0;  /* the size may have changed */
static volatile sig_atomic_t cterm__resumed = 0;         /* came back from being suspended */
static volatile sig_atomic_t cterm__entered = 0;         /* the terminal is in our modes */
static volatile sig_atomic_t cterm__mouse_mode = 0;

/* The sequence that turns on a mouse mode. */
static const char *cterm__mouse_on_sequence(int mode) {
    return mode == CTERM_MOUSE_BUTTONS ? "\x1b[?1002h\x1b[?1006h" :
           mode == CTERM_MOUSE_MOTION  ? "\x1b[?1003h\x1b[?1006h" : "";
}
#if defined(_WIN32)

/* ------------------------------ Windows ---------------------------------- */

/* Older compilers' headers may not have these, but they're only numbers, and the
 * console itself decides whether they work. */
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
  #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
  #define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
  #define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif

static HANDLE cterm__hin, cterm__hout;
static DWORD  cterm__saved_in_mode, cterm__saved_out_mode;
static UINT   cterm__saved_in_cp, cterm__saved_out_cp;

/* True if the console accepted ENABLE_VIRTUAL_TERMINAL_INPUT, meaning special
 * keys arrive as the same escape sequences a Unix terminal would send. */
static int cterm__vt_input;

/* Puts the console back the way it was, without touching any cterm state. It's used
 * when the program is about to die. */
static void cterm__emergency_restore(void) {
    DWORD written;
    if (!cterm__entered) return;
    cterm__entered = 0;
    WriteFile(cterm__hout, CTERM__LEAVE_SEQUENCE, (DWORD)strlen(CTERM__LEAVE_SEQUENCE), &written, NULL);
    SetConsoleMode(cterm__hin, cterm__saved_in_mode);
    SetConsoleMode(cterm__hout, cterm__saved_out_mode);
    SetConsoleCP(cterm__saved_in_cp);
    SetConsoleOutputCP(cterm__saved_out_cp);
}

/* Ctrl+C arrives as a key, but Ctrl+Break and closing the window still end the
 * process. Windows runs this on a separate thread when they happen. */
static BOOL WINAPI cterm__on_console_event(DWORD type) {
    (void)type;
    cterm__emergency_restore();
    return FALSE; /* carry on with the default, which ends the process */
}

static LPTOP_LEVEL_EXCEPTION_FILTER cterm__saved_crash_filter;

/* Runs when the program crashes (the Windows version of SIGSEGV and friends). */
static LONG WINAPI cterm__on_crash(EXCEPTION_POINTERS *info) {
    cterm__emergency_restore();
    if (cterm__saved_crash_filter) return cterm__saved_crash_filter(info);
    return EXCEPTION_CONTINUE_SEARCH;
}

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

    SetConsoleCtrlHandler(cterm__on_console_event, TRUE);
    cterm__saved_crash_filter = SetUnhandledExceptionFilter(cterm__on_crash);
    return 0;
}

static void cterm__plat_shutdown(void) {
    SetConsoleCtrlHandler(cterm__on_console_event, FALSE);
    SetUnhandledExceptionFilter(cterm__saved_crash_filter);
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

/* Writes the escape sequence for a special key that arrived as a Windows
 * "virtual key" instead of text. With ENABLE_VIRTUAL_TERMINAL_INPUT the console
 * already does this for us, so this is the fallback for consoles that lack it.
 * Going through escape sequences means there's still only one input parser.
 * Returns the bytes written (up to 16), or 0 for a key cterm doesn't use. */
static int cterm__win_special_key(WORD vk, DWORD control, unsigned char *out) {
    static const struct { WORD vk; char final; int tilde; } keys[] = {
        { VK_UP, 'A', 0 }, { VK_DOWN, 'B', 0 }, { VK_RIGHT, 'C', 0 }, { VK_LEFT, 'D', 0 },
        { VK_HOME, 'H', 0 }, { VK_END, 'F', 0 },
        { VK_F1, 'P', 0 }, { VK_F2, 'Q', 0 }, { VK_F3, 'R', 0 }, { VK_F4, 'S', 0 },
        { VK_INSERT, 0, 2 }, { VK_DELETE, 0, 3 }, { VK_PRIOR, 0, 5 }, { VK_NEXT, 0, 6 },
        { VK_F5, 0, 15 }, { VK_F6, 0, 17 }, { VK_F7, 0, 18 }, { VK_F8, 0, 19 },
        { VK_F9, 0, 20 }, { VK_F10, 0, 21 }, { VK_F11, 0, 23 }, { VK_F12, 0, 24 }
    };
    int mods = 1, i;
    if (control & SHIFT_PRESSED) mods += 1;
    if (control & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) mods += 2;
    if (control & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) mods += 4;

    /* Ctrl+Space reports no character on Windows. POSIX terminals send a NUL. */
    if (vk == VK_SPACE && (control & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))) {
        out[0] = 0;
        return 1;
    }

    /* These usually come with a character, but not on every console. */
    switch (vk) {
        case VK_ESCAPE: out[0] = CTERM__ESC; return 1;
        case VK_RETURN: out[0] = 0x0D; return 1;
        case VK_TAB:    out[0] = 0x09; return 1;
        case VK_BACK:   out[0] = 0x7F; return 1;
        default:        break;
    }
    for (i = 0; i < (int)(sizeof keys / sizeof keys[0]); i++) {
        if (keys[i].vk == vk) {
            char *text = (char *)out;
            if (keys[i].tilde) return snprintf(text, 16, "\x1b[%d;%d~", keys[i].tilde, mods);
            return snprintf(text, 16, "\x1b[1;%d%c", mods, keys[i].final);
        }
    }
    return 0;
}

static int cterm__plat_wait(unsigned char *buf, int cap, int timeout_ms) {
    INPUT_RECORD records[64];
    DWORD count = 0, i;
    int len = 0;

    switch (WaitForSingleObject(cterm__hin, timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms)) {
        case WAIT_OBJECT_0: break;
        case WAIT_TIMEOUT:  return 0;
        default:            return -1;
    }
    if (!ReadConsoleInputW(cterm__hin, records, 64, &count)) return -1;

    for (i = 0; i < count; i++) {
        if (records[i].EventType == WINDOW_BUFFER_SIZE_EVENT) {
            cterm__resize_pending = 1;
        } else if (records[i].EventType == KEY_EVENT) {
            const KEY_EVENT_RECORD *key = &records[i].Event.KeyEvent;
            WCHAR unit = key->uChar.UnicodeChar;
            int repeat = key->wRepeatCount;

            /* A character typed with Alt+numpad arrives on the release of Alt. */
            int down = key->bKeyDown || (key->wVirtualKeyCode == VK_MENU && unit != 0);
            if (!down) continue;

            for (; repeat > 0 && len + 16 <= cap; repeat--) {
                if (unit == 0) {
                    len += cterm__win_special_key(key->wVirtualKeyCode, key->dwControlKeyState, buf + len);
                } else {
                    if (!cterm__vt_input) {
                        /* Without VT input we have to build these ourselves. */
                        if (unit == '\t' && (key->dwControlKeyState & SHIFT_PRESSED)) {
                            memcpy(buf + len, "\x1b[Z", 3);
                            len += 3;
                            continue;
                        }
                        if (key->dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) {
                            buf[len++] = CTERM__ESC;
                        }
                    }
                    len += cterm__win_put_utf16(buf + len, unit);
                }
            }
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

static uint64_t cterm__plat_now_ms(void) {
    return GetTickCount64();
}

static int cterm__plat_suspend(void) {
    return cterm__fail("Windows has no job control, so cterm_suspend isn't available");
}

#else

/* ------------------------------- POSIX ----------------------------------- */

static struct termios cterm__saved_termios;

/* The "self-pipe trick". A signal can arrive just before select() starts waiting,
 * and select() would then sleep through it. So the handler also writes a byte to
 * this pipe, and select() watches the pipe. Even if the signal comes first, the
 * byte is still there, and select() wakes up at once. */
static int cterm__wake_pipe[2] = { -1, -1 };

/* Wakes up cterm__plat_wait. Safe to call from a signal handler. */
static void cterm__wake(void) {
    if (cterm__wake_pipe[1] >= 0) {
        char byte = 0;
        ssize_t ignored = write(cterm__wake_pipe[1], &byte, 1);
        (void)ignored; /* if the pipe is full, a wake-up is already on its way */
    }
}

/* --- Signals --------------------------------------------------------------
 *
 * Signal handlers may interrupt the program anywhere, including inside malloc or
 * printf, so they can only call a short list of "async-signal-safe" functions. Those
 * include write, tcsetattr, sigaction, sigprocmask and raise, and that's all that's
 * used below. Nothing here allocates memory or touches a cell buffer.
 */

static struct termios cterm__raw_termios;

/* Puts the terminal back to normal, for a crash or a suspend. */
static void cterm__emergency_restore(void) {
    ssize_t ignored;
    if (!cterm__entered) return;
    cterm__entered = 0;
    ignored = write(STDOUT_FILENO, CTERM__LEAVE_SEQUENCE, sizeof CTERM__LEAVE_SEQUENCE - 1);
    (void)ignored;
    /* TCSANOW, because waiting for output could hang if the terminal is stuck. */
    tcsetattr(STDIN_FILENO, TCSANOW, &cterm__saved_termios);
}

/* Takes the terminal again, after a suspend. */
static void cterm__emergency_reenter(void) {
    const char *mouse = cterm__mouse_on_sequence(cterm__mouse_mode);
    size_t mouse_length = 0;
    ssize_t ignored;
    while (mouse[mouse_length]) mouse_length++; /* strlen isn't on the safe list */
    tcsetattr(STDIN_FILENO, TCSANOW, &cterm__raw_termios);
    ignored = write(STDOUT_FILENO, CTERM__ENTER_SEQUENCE, sizeof CTERM__ENTER_SEQUENCE - 1);
    (void)ignored;
    ignored = write(STDOUT_FILENO, mouse, mouse_length);
    (void)ignored;
    cterm__entered = 1;
    cterm__resumed = 1;
    cterm__resize_pending = 1;
    cterm__wake();
}

/* Every signal cterm handles, with what was there before so it can be put back. */
#define CTERM__MAX_SIGNALS 12
static struct {
    int signal_number;
    struct sigaction saved;
} cterm__signals[CTERM__MAX_SIGNALS];
static int cterm__signal_count = 0;

static void cterm__on_winch(int sig) {
    int saved_errno = errno; /* the interrupted code may be about to check errno */
    (void)sig;
    cterm__resize_pending = 1;
    cterm__wake();
    errno = saved_errno;
}

/* The program is dying. Fix the terminal, then let the signal do what it would have
 * done anyway, so the exit status and any core dump are still correct. The signal is
 * blocked while this handler runs, so raise() only takes effect once it returns. */
static void cterm__on_fatal(int sig) {
    int i;
    cterm__emergency_restore();
    for (i = 0; i < cterm__signal_count; i++) {
        if (cterm__signals[i].signal_number == sig) {
            sigaction(sig, &cterm__signals[i].saved, NULL);
            break;
        }
    }
    raise(sig);
}

static volatile sig_atomic_t cterm__continued = 0;

/* SIGCONT: the user has resumed the app (or something sent it directly). */
static void cterm__on_cont(int sig) {
    int saved_errno = errno;
    (void)sig;
    cterm__continued = 1;
    cterm__emergency_reenter();
    errno = saved_errno;
}

/* SIGTSTP: asked to stop, by cterm_suspend or by something else. The terminal has to
 * be handed back first, and only then can the process really stop. To do that it
 * sends itself the same signal with the default action, and execution pauses inside
 * that raise() until SIGCONT arrives. */
static void cterm__on_tstp(int sig) {
    int saved_errno = errno;
    struct sigaction stop_action, this_action;
    sigset_t unblock;

    cterm__continued = 0;
    cterm__emergency_restore();

    memset(&stop_action, 0, sizeof stop_action);
    stop_action.sa_handler = SIG_DFL;
    sigemptyset(&stop_action.sa_mask);
    sigaction(SIGTSTP, &stop_action, &this_action);

    sigemptyset(&unblock);
    sigaddset(&unblock, SIGTSTP);
    sigprocmask(SIG_UNBLOCK, &unblock, NULL);

    raise(SIGTSTP);

    /* Resumed. The OS ignores stop signals in a group with no shell to resume it, in
     * which case raise() came straight back. Then SIGCONT never ran, so do its job. */
    sigprocmask(SIG_BLOCK, &unblock, NULL);
    sigaction(SIGTSTP, &this_action, NULL);
    if (!cterm__continued) cterm__emergency_reenter();
    errno = saved_errno;
    (void)sig;
}

/* Installs a handler, remembering what was there. */
static void cterm__install_signal(int sig, void (*handler)(int), int block_others) {
    struct sigaction action;
    if (cterm__signal_count >= CTERM__MAX_SIGNALS) return;
    memset(&action, 0, sizeof action);
    action.sa_handler = handler;
    if (block_others) sigfillset(&action.sa_mask);
    else sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    if (sigaction(sig, &action, &cterm__signals[cterm__signal_count].saved) == 0) {
        cterm__signals[cterm__signal_count].signal_number = sig;
        cterm__signal_count++;
    }
}

/* Installs a handler unless the signal is being ignored. Programs started with `nohup`
 * ignore SIGHUP on purpose, for example, and that choice has to be respected. */
static void cterm__install_unless_ignored(int sig, void (*handler)(int)) {
    struct sigaction current;
    if (sigaction(sig, NULL, &current) == 0 && current.sa_handler == SIG_IGN) return;
    cterm__install_signal(sig, handler, 1);
}

static void cterm__install_signals(void) {
    static const int fatal[] = { SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGABRT, SIGSEGV, SIGBUS, SIGFPE, SIGILL };
    size_t i;
    cterm__signal_count = 0;
    for (i = 0; i < sizeof fatal / sizeof fatal[0]; i++) cterm__install_unless_ignored(fatal[i], cterm__on_fatal);
    cterm__install_unless_ignored(SIGTSTP, cterm__on_tstp);
    cterm__install_signal(SIGCONT, cterm__on_cont, 0);
    cterm__install_signal(SIGWINCH, cterm__on_winch, 0);
}

static void cterm__restore_signals(void) {
    int i;
    for (i = cterm__signal_count - 1; i >= 0; i--) {
        sigaction(cterm__signals[i].signal_number, &cterm__signals[i].saved, NULL);
    }
    cterm__signal_count = 0;
}

static void cterm__close_wake_pipe(void) {
    if (cterm__wake_pipe[0] >= 0) close(cterm__wake_pipe[0]);
    if (cterm__wake_pipe[1] >= 0) close(cterm__wake_pipe[1]);
    cterm__wake_pipe[0] = -1;
    cterm__wake_pipe[1] = -1;
}

/* Creates the wake-up pipe. Non-blocking, so a full pipe can't hang the handler;
 * close-on-exec, so programs started by the app don't inherit it. */
static int cterm__open_wake_pipe(void) {
    int i;
    if (pipe(cterm__wake_pipe) != 0) {
        cterm__wake_pipe[0] = -1;
        cterm__wake_pipe[1] = -1;
        return cterm__fail("could not create a pipe");
    }
    for (i = 0; i < 2; i++) {
        fcntl(cterm__wake_pipe[i], F_SETFL, fcntl(cterm__wake_pipe[i], F_GETFL) | O_NONBLOCK);
        fcntl(cterm__wake_pipe[i], F_SETFD, fcntl(cterm__wake_pipe[i], F_GETFD) | FD_CLOEXEC);
    }
    /* select() can't watch descriptors this high. It would mean the app already
     * has over a thousand files open, so failing is the honest option. */
    if (cterm__wake_pipe[0] >= FD_SETSIZE) {
        cterm__close_wake_pipe();
        return cterm__fail("too many open files for select()");
    }
    return 0;
}

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

    /* VMIN=1, VTIME=0: read() returns as soon as there's at least one byte. It's
     * only called once select() says input is ready, so it never actually waits.
     * This also makes read() returning 0 mean one thing: the terminal has gone. */
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    /* TCSAFLUSH finishes pending output and discards keys typed before now, so
     * they don't get mistaken for input to the app. */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return cterm__fail("could not switch the terminal to raw mode");
    }
    cterm__raw_termios = raw;

    if (cterm__open_wake_pipe() != 0) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &cterm__saved_termios);
        return -1;
    }

    cterm__install_signals();
    return 0;
}

static void cterm__plat_shutdown(void) {
    cterm__restore_signals();
    cterm__close_wake_pipe();
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &cterm__saved_termios);
}

static int cterm__plat_wait(unsigned char *buf, int cap, int timeout_ms) {
    fd_set readable;
    struct timeval timeout;
    int ready;
    ssize_t n;

    FD_ZERO(&readable);
    FD_SET(STDIN_FILENO, &readable);
    FD_SET(cterm__wake_pipe[0], &readable);
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    /* select() rather than poll(), which is unreliable on terminals on macOS. */
    ready = select(cterm__wake_pipe[0] + 1, &readable, NULL, NULL, timeout_ms < 0 ? NULL : &timeout);
    if (ready < 0) return errno == EINTR ? 0 : -1;
    if (ready == 0) return 0;

    if (FD_ISSET(cterm__wake_pipe[0], &readable)) {
        char drain[64];
        ssize_t ignored = read(cterm__wake_pipe[0], drain, sizeof drain);
        (void)ignored;
    }
    if (!FD_ISSET(STDIN_FILENO, &readable)) return 0;

    n = read(STDIN_FILENO, buf, (size_t)cap);
    if (n < 0) return (errno == EINTR || errno == EAGAIN) ? 0 : -1;
    if (n == 0) return -1; /* the terminal has gone */
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

static uint64_t cterm__plat_now_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
}

static int cterm__plat_suspend(void) {
    raise(SIGTSTP); /* see cterm__on_tstp: this returns once the app is resumed */
    return 0;
}

#endif /* platform */

/* ========================================================================= *
 * LIFECYCLE
 * ========================================================================= */

/* All the state of the one terminal a process has. */
static struct {
    int active;
    cterm_buffer back, front;
    cterm__sb out;
    cterm__render_state render;
    int repaint; /* the next present must repaint everything */
    cterm_color_mode color_mode;
    cterm__parser parser;
    uint64_t pending_since; /* when bytes last arrived for an incomplete sequence */
} cterm__term;

/* Resizes both buffers to match the terminal. Returns 1 if the size changed, 0 if
 * it didn't, or -1 if out of memory. */
static int cterm__update_size(void) {
    int width, height;
    cterm__plat_size(&width, &height);
    if (width == cterm__term.back.width && height == cterm__term.back.height) return 0;
    if (cterm_buffer_resize(&cterm__term.back, width, height) != 0) return -1;
    if (cterm_buffer_resize(&cterm__term.front, width, height) != 0) return -1;
    cterm__term.repaint = 1;
    return 1;
}

CTERM_API int cterm_init(void) {
    /* Calling init twice must not save raw settings as the "normal" ones. */
    if (cterm__term.active) return 0;
    if (cterm__plat_init() != 0) return -1;

    if (cterm__update_size() < 0) {
        cterm__plat_shutdown();
        CTERM_FREE(cterm__term.back.cells);
        CTERM_FREE(cterm__term.front.cells);
        memset(&cterm__term, 0, sizeof cterm__term);
        return -1;
    }
    cterm__render_state_reset(&cterm__term.render);
    cterm__term.repaint = 1;
#if defined(_WIN32)
    /* Any Windows console that got this far handles escape sequences, and with them
     * true colour. TERM usually isn't set there, so it can't be asked. */
    cterm__term.color_mode = cterm__color_mode_from_env(getenv("NO_COLOR"), "truecolor", NULL);
#else
    cterm__term.color_mode = cterm__color_mode_from_env(getenv("NO_COLOR"), getenv("COLORTERM"), getenv("TERM"));
#endif
    cterm__term.active = 1;
    cterm__mouse_mode = CTERM_MOUSE_OFF;
    cterm__resize_pending = 0;
    cterm__resumed = 0;
    cterm__plat_write(CTERM__ENTER_SEQUENCE, strlen(CTERM__ENTER_SEQUENCE));
    cterm__entered = 1;

    /* So a plain exit() doesn't leave the terminal broken. atexit can't be undone, so
     * it's only registered once, however many times cterm_init runs. */
    {
        static int registered = 0;
        if (!registered) {
            registered = 1;
            atexit(cterm_shutdown);
        }
    }
    return 0;
}

CTERM_API void cterm_shutdown(void) {
    if (!cterm__term.active) return;
    cterm__term.active = 0;
    cterm__entered = 0;
    cterm__plat_write(CTERM__LEAVE_SEQUENCE, strlen(CTERM__LEAVE_SEQUENCE));
    cterm__plat_shutdown();

    /* Free everything, so a later cterm_init() starts from a clean slate. */
    CTERM_FREE(cterm__term.back.cells);
    CTERM_FREE(cterm__term.front.cells);
    cterm__sb_free(&cterm__term.out);
    cterm__parser_free(&cterm__term.parser);
    memset(&cterm__term, 0, sizeof cterm__term);
}

CTERM_API int cterm_suspend(void) {
    if (!cterm__term.active) return cterm__fail("cterm_init has not been called");
    return cterm__plat_suspend();
}

CTERM_API cterm_buffer *cterm_screen(void) {
    return cterm__term.active ? &cterm__term.back : NULL;
}

CTERM_API void cterm_redraw(void) {
    cterm__term.repaint = 1;
}

CTERM_API cterm_color_mode cterm_get_color_mode(void) {
    return cterm__term.color_mode;
}

CTERM_API void cterm_set_color_mode(cterm_color_mode mode) {
    cterm__term.color_mode = mode;
    cterm__term.repaint = 1; /* everything on screen used the old colours */
}

CTERM_API int cterm_present(void) {
    if (!cterm__term.active) return cterm__fail("cterm_init has not been called");

    /* If memory ran out during a resize, one buffer may have the new size and the
     * other the old. The renderer needs them to match, so fix that first or stop. */
    if (cterm__term.front.width != cterm__term.back.width || cterm__term.front.height != cterm__term.back.height) {
        if (cterm_buffer_resize(&cterm__term.front, cterm__term.back.width, cterm__term.back.height) != 0) return -1;
        cterm__term.repaint = 1;
    }

    cterm__term.out.len = 0;
    cterm__render(&cterm__term.out, &cterm__term.front, &cterm__term.back, &cterm__term.render,
                  cterm__term.color_mode, cterm__term.repaint);
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

/* ========================================================================= *
 * EVENT LOOP
 * ========================================================================= */

/* How long the parser's incomplete input may wait before being flushed. */
static uint64_t cterm__pending_limit(const cterm__parser *parser) {
    return parser->in_paste ? CTERM_PASTE_TIMEOUT_MS : CTERM_ESC_TIMEOUT_MS;
}

CTERM_API int cterm_poll(cterm_event *event, int timeout_ms) {
    cterm__parser *parser = &cterm__term.parser;
    uint64_t start;

    memset(event, 0, sizeof *event);
    if (!cterm__term.active) return cterm__fail("cterm_init has not been called");
    start = cterm__plat_now_ms();

    for (;;) {
        unsigned char bytes[4096];
        uint64_t now;
        int wait_ms = -1, got;

        /* A resize comes first: everything drawn from here on should use the new size. */
        if (cterm__resize_pending) {
            int changed, resumed = cterm__resumed;
            cterm__resize_pending = 0;
            cterm__resumed = 0;
            changed = cterm__update_size();
            if (changed < 0) return -1;
            if (resumed) cterm__term.repaint = 1; /* the shell has used the screen since */
            if (changed > 0 || resumed) {
                event->type = CTERM_EVENT_RESIZE;
                event->width = cterm__term.back.width;
                event->height = cterm__term.back.height;
                return 1;
            }
        }

        if (cterm__parser_next(parser, event, 0)) return 1;

        /* Work out how long to sleep: until the caller's deadline, or until an
         * incomplete sequence has waited long enough, whichever comes first. Time is
         * measured from when its bytes arrived, not from when this call started. That
         * way the Esc key still works in an app that polls with a timeout of 0. */
        now = cterm__plat_now_ms();
        if (timeout_ms >= 0) {
            uint64_t elapsed = now - start;
            wait_ms = elapsed >= (uint64_t)timeout_ms ? 0 : (int)((uint64_t)timeout_ms - elapsed);
        }
        if (cterm__parser_pending(parser)) {
            uint64_t limit = cterm__pending_limit(parser);
            uint64_t waited = now - cterm__term.pending_since;
            if (waited >= limit) {
                if (cterm__parser_next(parser, event, 1)) return 1;
                continue; /* the leftovers turned out to be ignorable: look again */
            }
            if (wait_ms < 0 || (uint64_t)wait_ms > limit - waited) wait_ms = (int)(limit - waited);
        }

        got = cterm__plat_wait(bytes, (int)sizeof bytes, wait_ms);
        if (got < 0) return cterm__fail("the terminal has gone away");
        now = cterm__plat_now_ms();
        if (got > 0) {
            cterm__parser_feed(parser, bytes, (size_t)got);
            cterm__term.pending_since = now;
            continue;
        }

        /* Nothing arrived. Stop if the caller's time is up, unless there's something
         * to deliver right now: a resize, or an incomplete sequence that's now due
         * (the top of the loop handles both). */
        if (timeout_ms >= 0 && now - start >= (uint64_t)timeout_ms && !cterm__resize_pending) {
            int flush_due = cterm__parser_pending(parser) &&
                            now - cterm__term.pending_since >= cterm__pending_limit(parser);
            if (!flush_due) return 0;
        }
    }
}

CTERM_API void cterm_set_mouse(cterm_mouse_mode mode) {
    static const char off[] = "\x1b[?1006l\x1b[?1003l\x1b[?1002l\x1b[?1000l";
    const char *on = cterm__mouse_on_sequence(mode);
    if (!cterm__term.active) return;
    cterm__mouse_mode = mode; /* remembered, so it can be turned back on after a suspend */
    cterm__plat_write(off, sizeof off - 1);
    cterm__plat_write(on, strlen(on));
}

#ifndef CTERM_NO_UI

/* ========================================================================= *
 * LAYOUT
 * ========================================================================= */

/* Does the work for both split directions, thinking only in terms of one axis. */
static void cterm__split(cterm_rect area, int gap, const int *sizes, int count, cterm_rect *out, int vertical) {
    int extent = vertical ? area.h : area.w;
    int position = vertical ? area.y : area.x;
    int available, remaining, total_weight = 0, leftover_given = 0, i;
    int fill_space;

    if (count <= 0) return;
    if (gap < 0) gap = 0;
    available = extent - gap * (count - 1);
    if (available < 0) available = 0;

    /* First the sizes that don't depend on anything else. */
    remaining = available;
    for (i = 0; i < count; i++) {
        if (sizes[i] < 0) {
            total_weight += -sizes[i];
        } else {
            int wanted = sizes[i] >= 1000000 ? available * (sizes[i] - 1000000) / 100 : sizes[i];
            remaining -= wanted;
        }
    }
    fill_space = remaining > 0 ? remaining : 0;

    /* Then share what's left, and hand out sizes from the start. `room` is what's
     * really left, so when things don't fit, later children come up short. */
    remaining = available;
    for (i = 0; i < count; i++) {
        int size;
        if (sizes[i] < 0) {
            size = (int)((long long)fill_space * -sizes[i] / total_weight);
            leftover_given += size;
        } else if (sizes[i] >= 1000000) {
            size = available * (sizes[i] - 1000000) / 100;
        } else {
            size = sizes[i];
        }
        if (size > remaining) size = remaining;
        remaining -= size;

        if (vertical) out[i] = cterm_rect_make(area.x, position, area.w, size);
        else out[i] = cterm_rect_make(position, area.y, size, area.h);
        position += size + gap;
    }

    /* Division rounds down, so a few cells can be left over. Give them to the first
     * fill children, moving everything after them along. */
    {
        int spare = fill_space - leftover_given;
        int shift = 0;
        for (i = 0; i < count; i++) {
            int extra = 0;
            if (sizes[i] < 0 && spare > 0) {
                extra = 1;
                spare--;
            }
            if (vertical) {
                out[i].y += shift;
                out[i].h += extra;
            } else {
                out[i].x += shift;
                out[i].w += extra;
            }
            shift += extra;
        }
    }
}

CTERM_API void cterm_split_rows(cterm_rect area, int gap, const int *sizes, int count, cterm_rect *out) {
    cterm__split(area, gap, sizes, count, out, 1);
}

CTERM_API void cterm_split_columns(cterm_rect area, int gap, const int *sizes, int count, cterm_rect *out) {
    cterm__split(area, gap, sizes, count, out, 0);
}

/* ========================================================================= *
 * WIDGET CORE
 *
 * How focus works without widget objects: every focusable widget adds itself to a
 * list as it's drawn, and two lists are kept, last frame's and this frame's. Tab and
 * mouse clicks are handled in cterm_ui_begin using last frame's list, because by the
 * time this frame's widgets run they all need to agree on who has the focus.
 * ========================================================================= */

#define CTERM__UI_WHEEL_STEP 3 /* rows moved per notch of the mouse wheel */

CTERM_API void cterm_ui_init(cterm_ui *ui) {
    memset(ui, 0, sizeof *ui);
    ui->theme.input                   = cterm_style_make(CTERM_DEFAULT, CTERM_DEFAULT, CTERM_UNDERLINE);
    ui->theme.input_focused           = cterm_style_make(CTERM_DEFAULT, CTERM_DEFAULT, CTERM_UNDERLINE | CTERM_BOLD);
    ui->theme.placeholder             = cterm_style_make(CTERM_DEFAULT, CTERM_DEFAULT, CTERM_UNDERLINE | CTERM_DIM);
    ui->theme.list_item               = cterm_plain();
    ui->theme.list_selected           = cterm_style_make(CTERM_DEFAULT, CTERM_DEFAULT, CTERM_REVERSE);
    ui->theme.list_selected_idle      = cterm_style_make(CTERM_DEFAULT, CTERM_DEFAULT, CTERM_BOLD);
    ui->theme.scrollbar_track         = cterm_style_make(CTERM_DEFAULT, CTERM_DEFAULT, CTERM_DIM);
    ui->theme.scrollbar_thumb         = cterm_style_make(CTERM_DEFAULT, CTERM_DEFAULT, CTERM_DIM);
    ui->theme.scrollbar_thumb_focused = cterm_plain();
}

/* Moves the focus one step through last frame's widgets. */
static void cterm__ui_move_focus(cterm_ui *ui, int backwards) {
    const cterm_ui_slot *slots = ui->widgets[ui->current];
    int count = ui->widget_count[ui->current];
    int index = -1, i;
    for (i = 0; i < count; i++) {
        if (slots[i].id == ui->focus) index = i;
    }
    if (index < 0) index = backwards ? 0 : -1; /* nothing focused: start from the edge */
    ui->focus = slots[(index + (backwards ? -1 : 1) + count) % count].id;
}

CTERM_API void cterm_ui_begin(cterm_ui *ui, cterm_buffer *buffer, const cterm_event *event) {
    ui->buffer = buffer;
    ui->event = event && event->type != CTERM_EVENT_NONE ? event : NULL;
    ui->event_used = 0;

    if (ui->event) {
        int count = ui->widget_count[ui->current];
        if (event->type == CTERM_EVENT_KEY && event->key == CTERM_KEY_TAB &&
            !(event->mods & (CTERM_MOD_CTRL | CTERM_MOD_ALT)) && count > 0) {
            cterm__ui_move_focus(ui, event->mods & CTERM_MOD_SHIFT);
            ui->event_used = 1;
        } else if (event->type == CTERM_EVENT_MOUSE && event->action == CTERM_MOUSE_PRESS) {
            /* The last match wins, because a widget inside a scroll view comes after
             * the view itself. The click isn't used up: the widget handles it too. */
            int i;
            for (i = 0; i < count; i++) {
                const cterm_ui_slot *slot = &ui->widgets[ui->current][i];
                if (cterm_rect_contains(slot->area, event->x, event->y)) ui->focus = slot->id;
            }
        }
    }

    /* Start collecting this frame's widgets in the other list. */
    ui->current = !ui->current;
    ui->widget_count[ui->current] = 0;
    ui->focus_seen = 0;
}

CTERM_API void cterm_ui_end(cterm_ui *ui) {
    /* If the focused widget wasn't drawn this frame, it's gone. Give the focus to the
     * first widget, which will show it from the next frame on. */
    if (ui->focus && !ui->focus_seen) {
        ui->focus = ui->widget_count[ui->current] > 0 ? ui->widgets[ui->current][0].id : NULL;
    }
    if (ui->event && ui->event->type == CTERM_EVENT_MOUSE && ui->event->action == CTERM_MOUSE_RELEASE) {
        ui->mouse_capture = NULL;
    }
    ui->buffer = NULL;
}

CTERM_API int cterm_ui_event_handled(const cterm_ui *ui) {
    return ui->event_used;
}

CTERM_API void cterm_ui_focus(cterm_ui *ui, const void *widget_state) {
    ui->focus = widget_state;
    ui->focus_seen = 1;
}

CTERM_API int cterm_ui_has_focus(const cterm_ui *ui, const void *widget_state) {
    return ui->focus == widget_state;
}

/* Adds a widget to this frame's list and returns 1 if it has the focus. */
static int cterm__ui_register(cterm_ui *ui, const void *id, cterm_rect area) {
    if (ui->widget_count[ui->current] < CTERM_UI_MAX_WIDGETS) {
        cterm_ui_slot *slot = &ui->widgets[ui->current][ui->widget_count[ui->current]++];
        slot->id = id;
        slot->area = cterm_rect_intersect(area, ui->buffer->clip); /* the part you can click */
    }
    if (!ui->focus) ui->focus = id;
    if (ui->focus == id) {
        ui->focus_seen = 1;
        return 1;
    }
    return 0;
}

/* This frame's event if it's unused and is a key or paste for a focused widget. */
static const cterm_event *cterm__ui_keyboard_event(const cterm_ui *ui, int focused) {
    if (!focused || !ui->event || ui->event_used) return NULL;
    if (ui->event->type != CTERM_EVENT_KEY && ui->event->type != CTERM_EVENT_PASTE) return NULL;
    return ui->event;
}

/* This frame's event if it's an unused mouse event over the visible part of `area`. */
static const cterm_event *cterm__ui_mouse_event(const cterm_ui *ui, cterm_rect area) {
    if (!ui->event || ui->event_used || ui->event->type != CTERM_EVENT_MOUSE) return NULL;
    if (!cterm_rect_contains(cterm_rect_intersect(area, ui->buffer->clip), ui->event->x, ui->event->y)) return NULL;
    return ui->event;
}

/* Narrows the clip area to also fit inside `area`. Returns the old one to restore. */
static cterm_rect cterm__ui_narrow_clip(cterm_buffer *buffer, cterm_rect area) {
    return cterm_set_clip(buffer, cterm_rect_intersect(area, buffer->clip));
}

static int cterm__clamp(int value, int low, int high) {
    if (value > high) value = high;
    if (value < low) value = low;
    return value;
}

/* Handles the scrollbar in a column. A press on the bar jumps there, and holding
 * the button drags. `*offset` runs from 0 to content - height. Returns 1 if it used
 * the event. */
static int cterm__ui_scrollbar_mouse(cterm_ui *ui, cterm_rect bar, const void *id, int content, int *offset) {
    const cterm_event *ev = ui->event;
    int max_offset = content - bar.h;
    int on_bar, captured;

    if (!ev || ui->event_used || ev->type != CTERM_EVENT_MOUSE || bar.h <= 0 || max_offset <= 0) return 0;

    on_bar = ev->action == CTERM_MOUSE_PRESS && cterm__ui_mouse_event(ui, bar) != NULL;
    captured = ui->mouse_capture == id && ev->action == CTERM_MOUSE_DRAG;
    if (!on_bar && !captured) return 0;

    if (on_bar) ui->mouse_capture = id;
    *offset = bar.h > 1 ? (ev->y - bar.y) * max_offset / (bar.h - 1) : 0;
    *offset = cterm__clamp(*offset, 0, max_offset);
    ui->event_used = 1;
    return 1;
}

/* Draws a scrollbar. The thumb shows which part of the content is visible, and its
 * size shows how much. */
static void cterm__ui_scrollbar_draw(cterm_ui *ui, cterm_rect bar, int content, int offset, int focused) {
    int max_offset = content - bar.h;
    int thumb_size, thumb_at, y;
    if (bar.h <= 0 || max_offset <= 0) return;

    thumb_size = (int)((long long)bar.h * bar.h / content);
    if (thumb_size < 1) thumb_size = 1;
    thumb_at = (int)((long long)offset * (bar.h - thumb_size) / max_offset);

    for (y = 0; y < bar.h; y++) {
        int in_thumb = y >= thumb_at && y < thumb_at + thumb_size;
        cterm_put(ui->buffer, bar.x, bar.y + y, in_thumb ? 0x2588 : 0x2502, /* full block, line */
                  in_thumb ? (focused ? ui->theme.scrollbar_thumb_focused : ui->theme.scrollbar_thumb)
                           : ui->theme.scrollbar_track);
    }
}

/* ========================================================================= *
 * TEXT INPUT
 *
 * The text is ordinary UTF-8, and `cursor` is a byte offset that only ever rests on
 * the boundary between characters. Two ideas of "one character" are used, because
 * people expect different things from different keys:
 *
 *   Moving and Delete work on a cluster, a character with its combining marks, so
 *   the cursor never lands inside something that looks like one letter.
 *
 *   Backspace removes one code point, so in scripts such as Bengali you can take back
 *   a vowel sign without losing the consonant it was typed onto.
 * ========================================================================= */

/* The code point at byte `at`. */
static uint32_t cterm__input_cp(const char *text, size_t length, size_t at, int *used) {
    uint32_t cp = CTERM__REPLACEMENT;
    int n = cterm__utf8_decode((const unsigned char *)text + at, length - at, &cp);
    if (n == 0) n = (int)(length - at); /* truncated at the end: take the rest */
    if (used) *used = n;
    return cp;
}

/* The start of the code point before byte `at` (at > 0). */
static size_t cterm__input_prev_char(const char *text, size_t at) {
    at--;
    while (at > 0 && ((unsigned char)text[at] & 0xC0) == 0x80) at--;
    return at;
}

/* The end of the cluster starting at byte `at`. */
static size_t cterm__input_next_cluster(const char *text, size_t length, size_t at) {
    int used;
    if (at >= length) return length;
    cterm__input_cp(text, length, at, &used);
    at += (size_t)used;
    while (at < length) {
        uint32_t cp = cterm__input_cp(text, length, at, &used);
        if (cterm_char_width(cp) != 0) break;
        at += (size_t)used;
    }
    return at;
}

/* The start of the cluster before byte `at`. */
static size_t cterm__input_prev_cluster(const char *text, size_t length, size_t at) {
    if (at == 0) return 0;
    at = cterm__input_prev_char(text, at);
    while (at > 0 && cterm_char_width(cterm__input_cp(text, length, at, NULL)) == 0) {
        at = cterm__input_prev_char(text, at);
    }
    return at;
}

/* The start of the word before byte `at`: skip spaces going left, then non-spaces. */
static size_t cterm__input_prev_word(const char *text, size_t at) {
    while (at > 0 && text[at - 1] == ' ') at--;
    while (at > 0 && text[at - 1] != ' ') at--;
    return at;
}

/* The end of the word after byte `at`: skip spaces going right, then non-spaces. */
static size_t cterm__input_next_word(const char *text, size_t length, size_t at) {
    while (at < length && text[at] == ' ') at++;
    while (at < length && text[at] != ' ') at++;
    return at;
}

/* Removes the bytes from `from` to `to` and leaves the cursor at `from`. */
static void cterm__input_erase(cterm_input *input, size_t length, size_t from, size_t to) {
    memmove(input->text + from, input->text + to, length - to + 1); /* + 1: the NUL */
    input->cursor = from;
}

/* Inserts UTF-8 at the cursor, stopping when it's full (never partway through a
 * character). Invalid bytes are dropped, and line breaks and tabs become spaces,
 * since this is a one-line field. All other control characters are dropped. Returns
 * 1 if anything changed. */
static int cterm__input_insert(cterm_input *input, const char *utf8, size_t n) {
    const unsigned char *s = (const unsigned char *)utf8;
    size_t length = strlen(input->text);
    int changed = 0;

    while (n > 0) {
        uint32_t cp = 0;
        char bytes[4];
        int used = cterm__utf8_decode(s, n, &cp), size;

        if (used == 0) break; /* a truncated character at the end: drop it */
        /* A valid U+FFFD is 3 bytes. A replacement for an invalid byte has used == 1. */
        if (cp == CTERM__REPLACEMENT && used == 1) goto next;

        if (cp == '\r' && n > 1 && s[1] == '\n') goto next; /* CR LF is a single line break */
        if (cp == '\r' || cp == '\n' || cp == '\t') cp = ' ';
        if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0)) goto next;

        size = cterm__utf8_encode(cp, bytes);
        if (length + (size_t)size + 1 > input->capacity) break;
        memmove(input->text + input->cursor + size, input->text + input->cursor, length - input->cursor + 1);
        memcpy(input->text + input->cursor, bytes, (size_t)size);
        input->cursor += (size_t)size;
        length += (size_t)size;
        changed = 1;

    next:
        s += used;
        n -= (size_t)used;
    }
    return changed;
}

CTERM_API void cterm_input_init(cterm_input *input, char *storage, size_t capacity) {
    memset(input, 0, sizeof *input);
    input->text = storage;
    input->capacity = capacity;
    if (storage && capacity > 0) storage[0] = '\0';
}

CTERM_API void cterm_input_set(cterm_input *input, const char *utf8) {
    if (!input->text || input->capacity == 0) return;
    input->text[0] = '\0';
    input->cursor = 0;
    input->scroll = 0;
    cterm__input_insert(input, utf8, strlen(utf8));
}

/* How many cells the text up to byte `end` takes on screen. */
static int cterm__input_columns(const cterm_input *input, size_t end) {
    int mask_width = input->mask ? cterm_char_width(input->mask) : 0;
    int columns = 0;
    size_t at = 0;
    while (at < end) {
        int used, width;
        uint32_t cp = cterm__input_cp(input->text, end, at, &used);
        width = cterm_char_width(cp);
        if (input->mask && width > 0) width = mask_width;
        columns += width;
        at += (size_t)used;
    }
    return columns;
}

/* The byte offset of the cluster at a screen column, for clicks. */
static size_t cterm__input_at_column(const cterm_input *input, size_t length, int column) {
    size_t at = 0;
    while (at < length) {
        size_t next = cterm__input_next_cluster(input->text, length, at);
        if (cterm__input_columns(input, next) > column) return at;
        at = next;
    }
    return length;
}

/* Handles a key or paste. Returns CTERM_UI_ flags, and sets *used if it recognised
 * the event. */
static int cterm__input_handle(cterm_input *input, const cterm_event *ev, int *used) {
    char *text = input->text;
    size_t length = strlen(text), cursor = input->cursor;
    int ctrl, alt, key, result = 0;

    *used = 1;

    if (ev->type == CTERM_EVENT_PASTE) {
        return cterm__input_insert(input, ev->text, ev->text_length) ? CTERM_UI_CHANGED : 0;
    }

    ctrl = (ev->mods & CTERM_MOD_CTRL) != 0;
    alt = (ev->mods & CTERM_MOD_ALT) != 0;
    key = ev->key;

    /* Plain text. Shift is fine: it's how capitals get typed. */
    if (key < CTERM_KEY_ENTER && key >= 0x20 && !ctrl && !alt) {
        char bytes[4];
        int size = cterm__utf8_encode((uint32_t)key, bytes);
        return cterm__input_insert(input, bytes, (size_t)size) ? CTERM_UI_CHANGED : 0;
    }

    /* Editing and movement. Anything that's not here goes back to the app. */
    if (ctrl && key == 'a') input->cursor = 0;
    else if (ctrl && key == 'e') input->cursor = length;
    else if (key == CTERM_KEY_HOME) input->cursor = 0;
    else if (key == CTERM_KEY_END) input->cursor = length;
    else if (key == CTERM_KEY_LEFT && ctrl) input->cursor = cterm__input_prev_word(text, cursor);
    else if (key == CTERM_KEY_RIGHT && ctrl) input->cursor = cterm__input_next_word(text, length, cursor);
    else if (key == 'b' && alt) input->cursor = cterm__input_prev_word(text, cursor);
    else if (key == 'f' && alt) input->cursor = cterm__input_next_word(text, length, cursor);
    else if (key == CTERM_KEY_LEFT) input->cursor = cterm__input_prev_cluster(text, length, cursor);
    else if (key == CTERM_KEY_RIGHT) input->cursor = cterm__input_next_cluster(text, length, cursor);
    else if (key == CTERM_KEY_ENTER) result = CTERM_UI_SUBMITTED;
    else {
        size_t from = cursor, to = cursor;
        if (key == CTERM_KEY_BACKSPACE && (ctrl || alt)) from = cterm__input_prev_word(text, cursor);
        else if (ctrl && key == 'w') from = cterm__input_prev_word(text, cursor);
        else if (key == CTERM_KEY_BACKSPACE) from = cursor > 0 ? cterm__input_prev_char(text, cursor) : 0;
        else if (key == CTERM_KEY_DELETE && ctrl) to = cterm__input_next_word(text, length, cursor);
        else if (key == CTERM_KEY_DELETE) to = cterm__input_next_cluster(text, length, cursor);
        else if (ctrl && key == 'u') from = 0;
        else if (ctrl && key == 'k') to = length;
        else {
            *used = 0;
            return 0;
        }
        if (from != to) {
            cterm__input_erase(input, length, from, to);
            result = CTERM_UI_CHANGED;
        }
    }
    return result;
}

CTERM_API int cterm_ui_input(cterm_ui *ui, cterm_rect area, cterm_input *input) {
    cterm_buffer *buffer = ui->buffer;
    const cterm_event *ev;
    cterm_rect row = cterm_rect_make(area.x, area.y, area.w, area.h > 0 ? 1 : 0);
    cterm_rect old_clip;
    size_t length;
    int focused, result = 0, cursor_column, total_columns;
    cterm_style style;

    if (!input->text || input->capacity == 0) return 0;
    focused = cterm__ui_register(ui, input, row);

    /* The app can change `text` whenever it likes, so make sure the cursor is still
     * inside it, and on the boundary of a character. */
    length = strlen(input->text);
    if (input->cursor > length) input->cursor = length;
    while (input->cursor > 0 && ((unsigned char)input->text[input->cursor] & 0xC0) == 0x80) input->cursor--;

    ev = cterm__ui_keyboard_event(ui, focused);
    if (ev) {
        int used;
        result = cterm__input_handle(input, ev, &used);
        if (used) ui->event_used = 1;
        length = strlen(input->text);
    }

    /* A click puts the cursor there. */
    ev = cterm__ui_mouse_event(ui, row);
    if (ev && ev->action == CTERM_MOUSE_PRESS) {
        input->cursor = cterm__input_at_column(input, length, ev->x - row.x + input->scroll);
        ui->event_used = 1;
    }

    /* Scroll just enough to keep the cursor visible. It needs a cell of its own, and
     * the field shouldn't sit half empty after text is deleted. */
    cursor_column = cterm__input_columns(input, input->cursor);
    total_columns = cterm__input_columns(input, length);
    if (cursor_column < input->scroll) input->scroll = cursor_column;
    if (cursor_column >= input->scroll + row.w) input->scroll = cursor_column - row.w + 1;
    if (input->scroll > total_columns - row.w + 1) input->scroll = total_columns - row.w + 1;
    if (input->scroll < 0) input->scroll = 0;

    old_clip = cterm__ui_narrow_clip(buffer, row);
    style = focused ? ui->theme.input_focused : ui->theme.input;
    cterm_fill(buffer, row, ' ', style);

    if (length == 0) {
        if (input->placeholder) cterm_text(buffer, row.x, row.y, ui->theme.placeholder, input->placeholder);
    } else if (input->mask) {
        int x = row.x - input->scroll;
        size_t at = 0;
        while (at < length) {
            int used;
            if (cterm_char_width(cterm__input_cp(input->text, length, at, &used)) > 0) {
                cterm_put(buffer, x, row.y, input->mask, style);
                x += cterm_char_width(input->mask);
            }
            at += (size_t)used;
        }
    } else {
        cterm_textn(buffer, row.x - input->scroll, row.y, style, input->text, length);
    }

    if (focused) cterm_show_cursor(buffer, row.x + cursor_column - input->scroll, row.y);
    cterm_set_clip(buffer, old_clip);
    return result;
}

/* ========================================================================= *
 * LIST
 * ========================================================================= */

/* Adapter so a plain array of strings can use the callback version. */
static const char *cterm__list_from_array(int index, void *user_data) {
    return ((const char *const *)user_data)[index];
}

CTERM_API int cterm_ui_list_fn(cterm_ui *ui, cterm_rect area, cterm_list *list, int count,
                               cterm_list_text_fn text, void *user_data) {
    cterm_buffer *buffer = ui->buffer;
    const cterm_event *ev;
    cterm_rect old_clip, rows, bar;
    int focused = cterm__ui_register(ui, list, area);
    int result = 0, previous, y, show_bar = count > area.h && area.w > 1;

    /* The numbers must make sense, even if the item count changed since last frame. */
    if (count < 0) count = 0;
    list->selected = cterm__clamp(list->selected, 0, count > 0 ? count - 1 : 0);
    previous = list->selected;

    rows = area;
    bar = cterm_rect_make(area.x + area.w - 1, area.y, 1, area.h);
    if (show_bar) rows.w = area.w - 1;

    ev = cterm__ui_keyboard_event(ui, focused);
    if (ev && ev->type == CTERM_EVENT_KEY && !(ev->mods & (CTERM_MOD_CTRL | CTERM_MOD_ALT)) && count > 0) {
        int page = area.h > 1 ? area.h - 1 : 1; /* keep one row of context */
        int used = 1;
        switch (ev->key) {
            case CTERM_KEY_UP:        list->selected -= 1; break;
            case CTERM_KEY_DOWN:      list->selected += 1; break;
            case CTERM_KEY_PAGE_UP:   list->selected -= page; break;
            case CTERM_KEY_PAGE_DOWN: list->selected += page; break;
            case CTERM_KEY_HOME:      list->selected = 0; break;
            case CTERM_KEY_END:       list->selected = count - 1; break;
            case CTERM_KEY_ENTER:     result |= CTERM_UI_SUBMITTED; break;
            default:                  used = 0; break;
        }
        if (used) ui->event_used = 1;
    }

    /* The scroll position can be moved by the scrollbar and by the wheel. */
    {
        int max_scroll = count > area.h ? count - area.h : 0;
        if (show_bar) cterm__ui_scrollbar_mouse(ui, bar, list, count, &list->scroll);

        ev = cterm__ui_mouse_event(ui, area); /* a press on the bar was already used above */
        if (ev) {
            if (ev->action == CTERM_MOUSE_SCROLL_UP) {
                list->scroll -= CTERM__UI_WHEEL_STEP;
                ui->event_used = 1;
            } else if (ev->action == CTERM_MOUSE_SCROLL_DOWN) {
                list->scroll += CTERM__UI_WHEEL_STEP;
                ui->event_used = 1;
            } else if (ev->action == CTERM_MOUSE_PRESS) {
                int row = list->scroll + ev->y - area.y;
                if (row < count) list->selected = row;
                ui->event_used = 1;
            }
        }
        list->scroll = cterm__clamp(list->scroll, 0, max_scroll);
        list->selected = cterm__clamp(list->selected, 0, count > 0 ? count - 1 : 0);

        /* Bring the selection into view when it has just moved, or the list has just
         * changed height. Otherwise leave it, so the wheel can look elsewhere. */
        if (list->selected != list->last_selected || list->selected != previous || area.h != list->last_height) {
            if (list->selected < list->scroll) list->scroll = list->selected;
            if (area.h > 0 && list->selected >= list->scroll + area.h) list->scroll = list->selected - area.h + 1;
            list->scroll = cterm__clamp(list->scroll, 0, max_scroll);
        }
    }
    if (list->selected != previous) result |= CTERM_UI_CHANGED;
    list->last_selected = list->selected;
    list->last_height = area.h;

    /* Draw only the visible rows, so a list of a million items costs the same. */
    for (y = 0; y < area.h; y++) {
        int index = list->scroll + y;
        cterm_rect row = cterm_rect_make(rows.x, rows.y + y, rows.w, 1);
        cterm_style style = ui->theme.list_item;
        if (index < count && index == list->selected) {
            style = focused ? ui->theme.list_selected : ui->theme.list_selected_idle;
        }
        old_clip = cterm__ui_narrow_clip(buffer, row);
        cterm_fill(buffer, row, ' ', style);
        if (index < count) {
            const char *label = text(index, user_data);
            if (label) cterm_text(buffer, row.x, row.y, style, label);
        }
        cterm_set_clip(buffer, old_clip);
    }

    if (show_bar) cterm__ui_scrollbar_draw(ui, bar, count, list->scroll, focused);
    return result;
}

CTERM_API int cterm_ui_list(cterm_ui *ui, cterm_rect area, cterm_list *list, const char *const *items, int count) {
    /* The cast only removes const for the generic pointer. The adapter puts it back. */
    return cterm_ui_list_fn(ui, area, list, count, cterm__list_from_array, (void *)(uintptr_t)items);
}

/* ========================================================================= *
 * SCROLLABLE VIEW
 * ========================================================================= */

CTERM_API cterm_rect cterm_ui_scroll_begin(cterm_ui *ui, cterm_rect area, cterm_scroll *scroll,
                                           int content_width, int content_height) {
    cterm_buffer *buffer = ui->buffer;
    const cterm_event *ev;
    cterm_rect content;
    int focused = 0, can_scroll, max_x, max_y, page;

    if (content_height < 0) content_height = 0;
    scroll->content_height = content_height;
    scroll->bar_visible = content_height > area.h && area.w > 1;

    /* A scrollbar takes one column away from the content. */
    scroll->viewport = area;
    if (scroll->bar_visible) scroll->viewport.w -= 1;
    if (content_width <= 0) content_width = scroll->viewport.w;

    max_x = content_width > scroll->viewport.w ? content_width - scroll->viewport.w : 0;
    max_y = content_height > area.h ? content_height - area.h : 0;
    can_scroll = max_x > 0 || max_y > 0;

    /* A view with nothing to scroll shouldn't be a stop on the Tab key's route. */
    if (can_scroll) focused = cterm__ui_register(ui, scroll, area);

    ev = cterm__ui_keyboard_event(ui, focused);
    page = area.h > 1 ? area.h - 1 : 1;
    if (ev && ev->type == CTERM_EVENT_KEY && !(ev->mods & (CTERM_MOD_CTRL | CTERM_MOD_ALT))) {
        int used = 1;
        switch (ev->key) {
            case CTERM_KEY_UP:        scroll->y -= 1; break;
            case CTERM_KEY_DOWN:      scroll->y += 1; break;
            case CTERM_KEY_LEFT:      scroll->x -= 1; break;
            case CTERM_KEY_RIGHT:     scroll->x += 1; break;
            case CTERM_KEY_PAGE_UP:   scroll->y -= page; break;
            case CTERM_KEY_PAGE_DOWN: scroll->y += page; break;
            case CTERM_KEY_HOME:      scroll->y = 0; scroll->x = 0; break;
            case CTERM_KEY_END:       scroll->y = max_y; break;
            default:                  used = 0; break;
        }
        if (used) ui->event_used = 1;
    }

    if (scroll->bar_visible) {
        cterm_rect bar = cterm_rect_make(area.x + area.w - 1, area.y, 1, area.h);
        cterm__ui_scrollbar_mouse(ui, bar, scroll, content_height, &scroll->y);
    }

    ev = cterm__ui_mouse_event(ui, area);
    if (ev && can_scroll) {
        int used = 1;
        switch (ev->action) {
            case CTERM_MOUSE_SCROLL_UP:    scroll->y -= CTERM__UI_WHEEL_STEP; break;
            case CTERM_MOUSE_SCROLL_DOWN:  scroll->y += CTERM__UI_WHEEL_STEP; break;
            case CTERM_MOUSE_SCROLL_LEFT:  scroll->x -= CTERM__UI_WHEEL_STEP; break;
            case CTERM_MOUSE_SCROLL_RIGHT: scroll->x += CTERM__UI_WHEEL_STEP; break;
            default:                       used = 0; break;
        }
        if (used) ui->event_used = 1;
    }

    scroll->x = cterm__clamp(scroll->x, 0, max_x);
    scroll->y = cterm__clamp(scroll->y, 0, max_y);

    scroll->saved_clip = cterm__ui_narrow_clip(buffer, scroll->viewport);
    content = cterm_rect_make(scroll->viewport.x - scroll->x, scroll->viewport.y - scroll->y,
                              content_width, content_height);
    return content;
}

CTERM_API void cterm_ui_scroll_end(cterm_ui *ui, cterm_scroll *scroll) {
    cterm_set_clip(ui->buffer, scroll->saved_clip);
    if (scroll->bar_visible) {
        cterm_rect bar = cterm_rect_make(scroll->viewport.x + scroll->viewport.w, scroll->viewport.y, 1, scroll->viewport.h);
        cterm__ui_scrollbar_draw(ui, bar, scroll->content_height, scroll->y, cterm_ui_has_focus(ui, scroll));
    }
}

#endif /* CTERM_NO_UI */

#endif /* CTERM_IMPLEMENTATION_DONE */
#endif /* CTERM_IMPLEMENTATION */

/*
MIT License

Copyright (c) 2026 sakif

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
