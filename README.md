# cterm

A terminal UI library for C and C++ that's a single header file. Copy `cterm.h` into your project and you can draw to the screen, handle keys and the mouse, and build interfaces with text inputs, lists and scrolling views. It has no dependencies and nothing to build.

```
╭─ Search ───────────────────────────────────────────────────╮
│an                                                          │
╰────────────────────────────────────────────────────────────╯
╭─ 7 matches ─────────────╮╭─ Details ───────────────────────╮
│Banana                   ││Banana                          █│
│Cranberry                ││                                █│
│Mango                    ││Line 1 of the details...        ││
│আম (mango, Bengali)      ││Line 2 of the details...        ││
╰─────────────────────────╯╰────────────────────────────────╯
```

**Part 1** of this README is for people using cterm in an app. **Part 2** is for anyone changing cterm itself.

---

# Part 1: Using cterm

## Quick start

Save this as `hello.c`, next to `cterm.h`:

```c
#define CTERM_IMPLEMENTATION
#include "cterm.h"

#include <stdio.h>

int main(void) {
    if (cterm_init() != 0) {
        fprintf(stderr, "cterm_init failed: %s\n", cterm_error());
        return 1;
    }

    for (;;) {
        cterm_buffer *screen = cterm_screen();
        cterm_event event;

        /* Draw the whole screen. cterm only sends the cells that changed. */
        cterm_clear(screen);
        cterm_text(screen, 2, 1, cterm_style_make(CTERM_GREEN, CTERM_DEFAULT, CTERM_BOLD), "Hello, world!");
        cterm_textf(screen, 2, 3, cterm_plain(), "This terminal is %d x %d. Press q to quit.", screen->width, screen->height);
        cterm_present();

        /* Wait for something to happen. */
        if (cterm_poll(&event, -1) < 0) break;
        if (event.type == CTERM_EVENT_KEY && event.key == 'q') break;
    }

    cterm_shutdown();
    return 0;
}
```

Then build and run it:

| Compiler | Command |
|---|---|
| gcc or clang | `cc -std=c99 hello.c -o hello && ./hello` |
| MSVC | `cl /utf-8 hello.c && hello.exe` |
| MinGW | `gcc hello.c -o hello.exe && hello.exe` |

To see more, run `make` and try `build/keys`, `build/dashboard` and `build/widgets`.

## Using it in a project

In **exactly one** `.c` file, put these two lines **before every other `#include`**:

```c
#define CTERM_IMPLEMENTATION
#include "cterm.h"
```

Every other file only needs `#include "cterm.h"`.

If you get a compile error saying the implementation must come first, that's what it's asking for. With a strict `-std=c99`, system headers included earlier would hide functions cterm needs.

## How an app is structured

Every cterm app is the same loop: **draw everything, present, wait for an event, repeat**.

You don't track what changed. Redraw the whole screen from your data each time, and cterm compares it with what's already on the terminal and sends only the difference. If nothing changed, it sends nothing.

### Things to know

- **Coordinates** count from 0, and (0, 0) is the top-left cell.
- **Quitting is your job.** Ctrl+C arrives as an ordinary key, so an app that ignores it can't be stopped that way. Always handle a quit key.
- **Crashes are covered.** The terminal is restored on `exit()`, on crashes and on fatal signals, so your shell won't be left broken.
- **Resizing blanks the screen.** After `CTERM_EVENT_RESIZE`, draw everything again. The usual loop does that anyway.
- **One thread.** Call cterm from a single thread only.
- **Stdin and stdout must both be a terminal.** Otherwise `cterm_init` fails, so `myapp | less` won't work.

## Drawing

Get the screen with `cterm_screen()`, then draw on it:

```c
cterm_buffer *s = cterm_screen();
cterm_style title = cterm_style_make(CTERM_BRIGHT_WHITE, CTERM_BLUE, CTERM_BOLD);

cterm_clear(s);
cterm_text(s, 0, 0, title, "Report");                           /* UTF-8 text */
cterm_textf(s, 0, 1, cterm_plain(), "%d items", 42);            /* printf-style */
cterm_put(s, 5, 2, 0x2605, cterm_plain());                      /* one character */
cterm_fill(s, cterm_rect_make(0, 3, 10, 2), '#', cterm_plain()); /* a filled area */
cterm_box(s, cterm_rect_make(0, 5, 20, 4), CTERM_BORDER_ROUNDED, cterm_plain());
cterm_hline(s, 0, 9, 20, CTERM_BORDER_SINGLE, cterm_plain());
cterm_present();
```

`cterm_text` returns the x position after the text, so you can chain calls:

```c
int x = cterm_text(s, 0, 0, bold, "Name: ");
cterm_text(s, x, 0, cterm_plain(), name);
```

Text doesn't wrap. Anything past the edge is cut off. Drawing off-screen is always safe.

### Colours and styles

```c
cterm_style a = cterm_style_make(CTERM_RED, CTERM_DEFAULT, 0);
cterm_style b = cterm_style_make(CTERM_COLOR256(208), CTERM_BLACK, CTERM_UNDERLINE);
cterm_style c = cterm_style_make(CTERM_RGB(255, 128, 0), CTERM_DEFAULT, CTERM_BOLD | CTERM_ITALIC);
```

Use any colour on any terminal. cterm works out what the terminal supports, and converts each colour to the nearest one it can show. It respects the [`NO_COLOR`](https://no-color.org) convention, and you can override the result with `cterm_set_color_mode`.

Attributes are `CTERM_BOLD`, `CTERM_DIM`, `CTERM_ITALIC`, `CTERM_UNDERLINE`, `CTERM_BLINK`, `CTERM_REVERSE` and `CTERM_STRIKE`. Not every terminal shows all of them.

### Clipping

To stop drawing from spilling out of an area, set a clip rectangle, then put the old one back:

```c
cterm_rect old = cterm_set_clip(s, panel);
cterm_text(s, panel.x, panel.y, cterm_plain(), very_long_text);
cterm_set_clip(s, old);
```

### Untrusted text

It's safe to draw text from files or the network. Control characters are shown as `�`, so they can't reach the terminal as commands.

### Cursor

The cursor is hidden by default. To show it, call this each frame after `cterm_clear`:

```c
cterm_show_cursor(s, x, y);
```

## Events

`cterm_poll(&event, timeout_ms)` waits for input. A timeout of `-1` waits forever, and `0` returns at once, which suits animation loops. It returns 1 if there's an event, 0 if time ran out, and -1 if the terminal has gone away.

```c
cterm_event ev;
if (cterm_poll(&ev, -1) < 0) break;

switch (ev.type) {
    case CTERM_EVENT_KEY:
        if (ev.key == CTERM_KEY_UP) move_up();
        if (ev.key == 's' && (ev.mods & CTERM_MOD_CTRL)) save();
        if (ev.key == 0x995) { /* a Bengali letter: text keys hold their code point */ }
        break;
    case CTERM_EVENT_MOUSE:
        if (ev.action == CTERM_MOUSE_PRESS) click(ev.x, ev.y);
        if (ev.action == CTERM_MOUSE_SCROLL_DOWN) scroll_down();
        break;
    case CTERM_EVENT_PASTE:
        insert_text(ev.text, ev.text_length); /* only valid until the next cterm_poll */
        break;
    case CTERM_EVENT_RESIZE:
        break; /* the next draw uses the new size automatically */
    default:
        break;
}
```

- **Keys:** text keys hold their Unicode code point, and everything else is a `CTERM_KEY_` value such as `CTERM_KEY_ENTER` or `CTERM_KEY_F1`. If a key isn't doing what you expect, run `build/keys` to see what cterm receives.
- **Mouse:** reporting is off by default, because it stops users selecting text with the mouse. Turn it on with `cterm_set_mouse(CTERM_MOUSE_BUTTONS)`.
- **Ctrl+Z:** this arrives as a key. To suspend the way terminal apps normally do, call `cterm_suspend()` when you see it. It does nothing on Windows.

## Layout

Split a rectangle into rows or columns, and nest the results:

```c
int row_sizes[] = { CTERM_FIXED(1), CTERM_FILL(1), CTERM_FIXED(1) };
int column_sizes[] = { CTERM_PERCENT(30), CTERM_FILL(1) };
cterm_rect rows[3], columns[2];

cterm_split_rows(cterm_bounds(screen), 0, row_sizes, 3, rows);
cterm_split_columns(rows[1], 0, column_sizes, 2, columns);
/* rows[0] is a header, columns[0] a sidebar, columns[1] the main area, rows[2] a footer */
```

| Size | Meaning |
|---|---|
| `CTERM_FIXED(n)` | Exactly `n` cells |
| `CTERM_PERCENT(p)` | `p` percent of the space |
| `CTERM_FILL(w)` | A share of what's left, in proportion to `w` |

The second argument adds that many empty cells between children. If there isn't enough room, later children get less, down to nothing.

## Widgets

Widgets are immediate-mode: there are no widget objects. Each frame, you call a function that draws the widget, handles the current event, and tells you what happened. Your app owns the data, and there's no second copy to keep in sync.

```c
#define CTERM_IMPLEMENTATION
#include "cterm.h"

#include <stdio.h>
#include <string.h>

static const char *const fruit[] = { "Apple", "Banana", "Cherry", "Date" };

int main(void) {
    /* Widget state has to stay in one place between frames. */
    static cterm_ui ui;
    static char name_text[64];
    static cterm_input name;
    static cterm_list list;
    cterm_event ev;

    if (cterm_init() != 0) {
        fprintf(stderr, "cterm_init failed: %s\n", cterm_error());
        return 1;
    }
    cterm_set_mouse(CTERM_MOUSE_BUTTONS);
    cterm_ui_init(&ui);
    cterm_input_init(&name, name_text, sizeof name_text);
    name.placeholder = "Your name";

    memset(&ev, 0, sizeof ev);
    for (;;) {
        cterm_buffer *s = cterm_screen();
        int sizes[] = { CTERM_FIXED(1), CTERM_FILL(1) };
        cterm_rect rows[2];
        cterm_split_rows(cterm_bounds(s), 1, sizes, 2, rows);

        cterm_clear(s);
        cterm_ui_begin(&ui, s, &ev);

        if (cterm_ui_input(&ui, rows[0], &name) & CTERM_UI_SUBMITTED) {
            cterm_ui_focus(&ui, &list); /* Enter moves on to the list */
        }
        if (cterm_ui_list(&ui, rows[1], &list, fruit, 4) & CTERM_UI_SUBMITTED) {
            break; /* Enter on a row finishes */
        }

        cterm_ui_end(&ui);
        cterm_present();

        if (cterm_poll(&ev, -1) < 0) break;
        if (ev.type == CTERM_EVENT_KEY && ev.key == 'q' && (ev.mods & CTERM_MOD_CTRL)) break;
    }

    cterm_shutdown();
    printf("%s picked %s\n", name_text, fruit[list.selected]);
    return 0;
}
```

The rules for widgets:

- **Call widgets between `cterm_ui_begin` and `cterm_ui_end`.**
- **State must not move between frames**, because a widget is identified by its state's address. Use `static` variables or a long-lived struct, not a local variable in the drawing function.
- **Tab and Shift+Tab** move between widgets, in the order you draw them. Clicking also moves the focus.
- **Keys go to the focused widget.** Use `cterm_ui_event_handled(&ui)` to check whether a widget used an event, if you want it for yourself.

### Text input

`cterm_input` is one line of editable UTF-8 text in storage you provide. Read `input.text` at any time. To change it, use `cterm_input_set`.

| Keys | Action |
|---|---|
| Left, Right, Home, End | Move |
| Ctrl+A, Ctrl+E | Go to the start or end |
| Ctrl+Left, Ctrl+Right, Alt+B, Alt+F | Move by word |
| Backspace, Delete | Delete a character |
| Ctrl+W, Alt+Backspace, Ctrl+Delete | Delete a word |
| Ctrl+U, Ctrl+K | Delete to the start or end |
| Enter | Returns `CTERM_UI_SUBMITTED` |

Optional fields: `input.placeholder` is hint text, and `input.mask` draws a character such as `'*'` in place of the text, for passwords.

### List

`cterm_ui_list` draws an array of strings. A zeroed `cterm_list` is ready to use, and `list.selected` is the selected row. It returns `CTERM_UI_CHANGED` when the selection moves, and `CTERM_UI_SUBMITTED` on Enter.

For huge or computed lists, use `cterm_ui_list_fn`. It asks for each row's text only when that row is visible, so a million rows cost the same as ten.

### Scrollable view

Pass the full size of your content, then draw it as if it all fits, relative to the rectangle you get back:

```c
static cterm_scroll scroll;

cterm_rect c = cterm_ui_scroll_begin(&ui, area, &scroll, 0, line_count);
for (int i = 0; i < line_count; i++) {
    cterm_text(s, c.x, c.y + i, cterm_plain(), lines[i]);
}
cterm_ui_scroll_end(&ui, &scroll);
```

A `content_width` of 0 means "as wide as the view". The arrows, PageUp and PageDown, Home and End, the mouse wheel and the scrollbar all work. For long content, skip lines that aren't visible and it stays fast.

### Theme

Colours live in `ui.theme`. The defaults use only attributes, so they work with no colour support:

```c
ui.theme.list_selected = cterm_style_make(CTERM_BLACK, CTERM_BRIGHT_CYAN, 0);
```

## Options

Define these before including the header. Full details are at the top of `cterm.h`.

| Macro | Where | Effect |
|---|---|---|
| `CTERM_NO_UI` | Every file | Leaves out layout and widgets |
| `CTERM_MAX_MARKS` | Every file | Combining marks per character (default 3) |
| `CTERM_UI_MAX_WIDGETS` | Every file | Focusable widgets per frame (default 128) |
| `CTERM_STATIC` | One file | Makes everything private to that file. Only use this if all cterm calls are in that one file |
| `CTERM_MALLOC`, `CTERM_REALLOC`, `CTERM_FREE` | One file | Your own allocator (define all three) |
| `CTERM_ESC_TIMEOUT_MS` | One file | How long a lone ESC waits to be the Esc key (default 50) |

## Platform support

The code targets Linux, macOS, the BSDs and Windows 10 or newer, on any terminal that understands standard escape sequences. That covers almost all modern terminals, including Windows Terminal and the Windows 10 console.

**What has actually been tested so far:**

| Platform | Status |
|---|---|
| Linux (gcc, clang, g++, sanitizers) | All tests pass, locally and in CI, including in real pseudo-terminals |
| Windows (MinGW) | All tests pass in CI, including in a real console through ConPTY |
| Windows (MSVC, as C and C++) | All tests pass in CI, including in a real console through ConPTY |
| macOS (clang) | Unit tests, the C++ build and link modes pass in CI. **The pseudo-terminal tests haven't passed yet** |
| BSDs | Not tested, and not in CI |

Nobody has yet looked at the output on a real Windows or macOS screen. The tests check what the program sends and receives, not how it looks. The CI workflow is in `.github/workflows/ci.yml`.

## Limitations

- **Complex scripts depend on your terminal.** cterm stores combining marks correctly and keeps the layout from drifting, but how Bengali, Arabic or similar text looks is up to the terminal. Many terminals don't shape these scripts well.
- **No full grapheme clusters.** Emoji joined with a zero-width joiner (such as family emoji) are treated as separate characters, and might look wider than the terminal draws them.
- **Right-to-left text** isn't reordered.
- **Widgets:** there's only text input, list and scrollable view so far. The wheel always scrolls the outermost scroll view.
- **Windows:** mintty (Git Bash's default window) needs `winpty` in front of the command, because it isn't a real console. Mouse input relies on the console's escape sequence support.

## Troubleshooting

**My terminal is stuck after a crash.** cterm handles almost every case, but not `kill -9` or a stack overflow. Type `reset` and press Enter. If Enter doesn't work, type `stty sane` and press Ctrl+J.

**A key doesn't work.** Run `build/keys` to see what's really arriving. Terminals and multiplexers sometimes capture keys before your app sees them.

**Characters are misaligned.** Your terminal and cterm disagree on a character's width, which is most common with emoji. `build/dashboard` is a quick way to check.

---

# Part 2: Developing cterm

## Layout of the repository

| Path | Contents |
|---|---|
| `cterm.h` | The whole library. There's a map of its sections at the top |
| `demo/` | Example programs, which also serve as manual tests |
| `tests/test_*.c` | Unit tests, which need no terminal |
| `tests/vt.h` | A small terminal emulator, used to check the renderer's output |
| `tests/e2e.py`, `tests/e2e_app.c` | End-to-end tests in a real pseudo-terminal (POSIX) |
| `tests/e2e_conpty.c` | The same idea for Windows, using ConPTY |
| `tests/link/` | Checks each documented way of including the header |
| `tests/check_readme.py` | Compiles this README's examples and checks the names it uses |
| `tools/gen_width.py` | Generates the Unicode width tables |
| `.github/workflows/ci.yml` | Runs everything on Linux, macOS and Windows |

## How it works

Output and input are both pipelines, with the OS-specific code only at the very end:

```
OUTPUT   app draws -> back buffer -> RENDERER (compares with front buffer) -> bytes -> PLATFORM -> terminal
INPUT    terminal -> PLATFORM -> bytes -> INPUT PARSER -> events -> cterm_poll -> app
```

The design rests on a few decisions. They're worth keeping, and each has a reason:

**The renderer and parser don't touch a terminal.** The renderer only appends to a string, and the parser only takes bytes. That's why nearly everything can be tested on any machine, with no terminal, under sanitizers, and with random input.

**Escape sequences only, on every OS.** Windows uses the same output sequences as Unix, and its input is converted into the same byte stream, so there's only one renderer and one parser. The cost is needing Windows 10 or newer. Don't add a code path based on the old Windows console API, because it would need its own renderer and parser.

**No terminfo.** Nearly every terminal in use today understands the same sequences, and depending on the terminfo database would be a big cost for little gain.

**Wide characters are always kept whole.** A wide character is a lead cell plus a continuation cell (`ch == 0`). Overwriting either half blanks the other. The renderer depends on this, and it's the reason for `cterm__split_wide`.

**Only the renderer's cursor tracking trusts a character's width.** Terminals disagree about widths, so `cterm__width_is_disputed` makes the renderer re-position after risky characters. That limits a disagreement to one cell, not a whole row.

**The parser has no clock.** Telling the Esc key from the start of a sequence takes a timeout, but the parser only has a `flush` argument. `cterm_poll` owns the timing, so parser tests can't be flaky.

**Signal handlers only use async-signal-safe calls.** They can interrupt anything, including `malloc`. See the comment above the signal code in `cterm.h` before changing it, and don't add `printf`, `malloc` or `strlen` there.

**Focus uses last frame's widget list.** Tab and clicks are resolved in `cterm_ui_begin`, using the list from the previous frame, so every widget in a frame agrees on who has the focus. That's why there's no one-frame lag.

## Building and testing

You need a C compiler, `make`, and Python 3 for the end-to-end tests.

| Command | What it does |
|---|---|
| `make` | Builds the demos into `build/` |
| `make test` | Runs the unit tests |
| `make test-all` | Unit tests with gcc, clang and C++, sanitizers, and link modes |
| `make e2e` | Runs the tests in a real pseudo-terminal |
| `make check-readme` | Compiles this README's examples and checks its names |
| `make windows` | Cross-compiles for Windows (needs MinGW) |
| `make test-windows` | Runs the Windows unit tests under Wine |
| `make e2e-windows` | Runs the ConPTY test under Wine, from a real terminal |
| `tests\build_msvc.bat` | Builds and runs everything with MSVC, from a Developer Command Prompt |

**Before committing,** run `make test-all e2e check-readme`. If you've touched anything Windows-related, run `make test-windows` too.

Debian and Ubuntu packages: `gcc-mingw-w64-x86-64` for the cross-compiler, and `wine` to run the result.

### Known Wine differences

Wine is useful, but it isn't Windows. `tests/e2e_conpty.c` skips two checks there, because Wine 10 gets them wrong even in programs that don't use cterm: it drops Ctrl+C, after which console waits stop working, and `ResizePseudoConsole` has no effect. Wine's console also doesn't interpret escape sequences as Windows 10 does, so it can't show how the output looks. Only trust real Windows for those.

## Writing tests

Tests are C files that include the implementation, so they can call internal `cterm__` functions. See the top of `tests/test.h`.

- **New key or sequence:** add it to `tests/test_parser.c`. The `KEY` macro automatically tries it split at every byte position.
- **Renderer change:** add a case to `tests/test_render.c`, and keep `test_random_frames_always_match` passing. It's the most important renderer test.
- **Widget change:** add to `tests/test_ui.c`. Widgets draw on an off-screen buffer with scripted events. The same approach works for testing your own app's UI.
- **Anything to do with signals, timing or real terminals:** add to `tests/e2e.py`.

## Common changes

### Add a new key

1. Add the name to the `CTERM_KEY_` enum. Add it at the end, because the order is part of the API. The list in `demo/keys.c` needs updating to match.
2. Map its sequence in `cterm__csi_tilde`, `cterm__csi_letter` or `cterm__parse_ss3`.
3. If Windows reports it as a virtual key, add it to `cterm__win_special_key`.
4. Add a test using the `KEY` macro.

### Add a widget

Follow `cterm_ui_list_fn` as a template:

1. Define a state struct where zeros are a valid start. Its address is the widget's identity.
2. Call `cterm__ui_register` to take part in focus.
3. Use `cterm__ui_keyboard_event` and `cterm__ui_mouse_event` to get events, and set `ui->event_used` for those you use.
4. Clamp your state first, because the app may have changed it, or the content may have shrunk.
5. Draw inside a narrowed clip (`cterm__ui_narrow_clip`), and restore it afterwards.
6. Add it to the `CTERM_NO_UI` block, and write off-screen tests, including for sizes down to 0x0.

### Port to a new OS

Implement the functions listed in the comment at the top of the `PLATFORM LAYER` section: `cterm__plat_init`, `_shutdown`, `_wait`, `_write`, `_size`, `_now_ms` and `_suspend`, plus `cterm__emergency_restore`. Nothing else in the file depends on the OS. A POSIX-like system probably only needs the feature-test macros at the top of the file adjusted.

### Update the Unicode tables

The tables are generated from the Unicode data in your Python install. After upgrading Python:

```sh
python3 tools/gen_width.py
```

Don't edit the tables by hand. If a character's width is wrong, change the rules in that script.

## Making a release

1. Make sure CI is green on all platforms, including real Windows and macOS.
2. Run the demos by hand on at least one real terminal per OS. Automated tests can't judge how it looks.
3. Update the platform table above with what's been tested.
4. Tag the commit. Users only need `cterm.h`, so that one file is the release.

## Licence

MIT. See [LICENSE](LICENSE), which is also at the end of `cterm.h` so that the licence travels with the file.
