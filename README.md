# TIC

TIC (short for "TUI in C") is a terminal UI library for C and C++ that's a single header file. Copy `tic.h` into your project and you can draw to the screen, handle keys and the mouse, and build interfaces with text inputs, lists and scrolling views. It has no dependencies and nothing to build.

```
╭─ Search ───────────────────────────────────────────────────╮
│an                                                          │
╰────────────────────────────────────────────────────────────╯
╭─ 6 matches ─────────────╮╭─ Details ───────────────────────╮
│Banana                   ││Banana                          █│
│Cranberry                ││                                █│
│Mango                    ││Line 1 of the details...        █│
│Orange                   ││Line 2 of the details...        ││
│Pomegranate              ││Line 3 of the details...        ││
│Tangerine                ││Line 4 of the details...        ││
╰─────────────────────────╯╰─────────────────────────────────╯
```

**Part 1** of this README is for people using TIC in an app. **Part 2** is for anyone changing TIC itself.

---

# Part 1: Using TIC

## Quick start

Save this as `hello.c`, next to `tic.h`:

```c
#define TIC_IMPLEMENTATION
#include "tic.h"

#include <stdio.h>

int main(void) {
    if (tic_init() != 0) {
        fprintf(stderr, "tic_init failed: %s\n", tic_error());
        return 1;
    }

    for (;;) {
        tic_buffer *screen = tic_screen();
        tic_event event;

        /* Draw the whole screen. TIC only sends the cells that changed. */
        tic_clear(screen);
        tic_text(screen, 2, 1, tic_style_make(TIC_GREEN, TIC_DEFAULT, TIC_BOLD), "Hello, world!");
        tic_textf(screen, 2, 3, tic_plain(), "This terminal is %d x %d. Press q to quit.", screen->width, screen->height);
        tic_present();

        /* Wait for something to happen. */
        if (tic_poll(&event, -1) < 0) break;
        if (event.type == TIC_EVENT_KEY && event.key == 'q') break;
    }

    tic_shutdown();
    return 0;
}
```

Then build and run it:

| Compiler | Command |
|---|---|
| gcc or clang (Linux, macOS) | `cc -std=c99 hello.c -o hello && ./hello` |
| MSVC (Windows) | `cl /utf-8 hello.c`, then `.\hello.exe` |
| MinGW (Windows) | `gcc hello.c -o hello.exe`, then `.\hello.exe` |

On Windows, run those as two separate commands, because Windows PowerShell doesn't understand `&&`. They work the same in Command Prompt and PowerShell.

To see more, run `make` and try `build/keys`, `build/dashboard` and `build/widgets`.

**Looking something up?** The [API reference](reference.md) documents every function, type and option.

**New to this?** Follow the step-by-step tutorial: [Build a system monitor](tutorial/system-monitor/README.md). It assumes no experience with terminal programs. More tutorials will be listed in [tutorial/](tutorial/README.md).

## Using it in a project

In **exactly one** `.c` file, put these two lines **before every other `#include`**:

```c
#define TIC_IMPLEMENTATION
#include "tic.h"
```

Every other file only needs `#include "tic.h"`.

If you get a compile error saying the implementation must come first, that's what it's asking for. With a strict `-std=c99`, system headers included earlier would hide functions TIC needs.

## How an app is structured

Every TIC app is the same loop: **draw everything, present, wait for an event, repeat**.

You don't track what changed. Redraw the whole screen from your data each time, and TIC compares it with what's already on the terminal and sends only the difference. If nothing changed, it sends nothing.

### Things to know

- **Coordinates** count from 0, and (0, 0) is the top-left cell.
- **Quitting is your job.** Ctrl+C arrives as an ordinary key, so an app that ignores it can't be stopped that way. Always handle a quit key.
- **Crashes are covered.** The terminal is restored on `exit()`, on crashes and on fatal signals, so your shell won't be left broken.
- **Resizing blanks the screen.** After `TIC_EVENT_RESIZE`, draw everything again. The usual loop does that anyway.
- **One thread.** Call TIC from a single thread only.
- **Stdin and stdout must both be a terminal.** Otherwise `tic_init` fails, so `myapp | less` won't work.

## Drawing

Get the screen with `tic_screen()`, then draw on it:

```c
tic_buffer *s = tic_screen();
tic_style title = tic_style_make(TIC_BRIGHT_WHITE, TIC_BLUE, TIC_BOLD);

tic_clear(s);
tic_text(s, 0, 0, title, "Report");                           /* UTF-8 text */
tic_textf(s, 0, 1, tic_plain(), "%d items", 42);            /* printf-style */
tic_put(s, 5, 2, 0x2605, tic_plain());                      /* one character */
tic_fill(s, tic_rect_make(0, 3, 10, 2), '#', tic_plain()); /* a filled area */
tic_box(s, tic_rect_make(0, 5, 20, 4), TIC_BORDER_ROUNDED, tic_plain());
tic_hline(s, 0, 9, 20, TIC_BORDER_SINGLE, tic_plain());
tic_present();
```

`tic_text` returns the x position after the text, so you can chain calls:

```c
int x = tic_text(s, 0, 0, bold, "Name: ");
tic_text(s, x, 0, tic_plain(), name);
```

Text doesn't wrap. Anything past the edge is cut off. Drawing off-screen is always safe.

### Colours and styles

```c
tic_style a = tic_style_make(TIC_RED, TIC_DEFAULT, 0);
tic_style b = tic_style_make(TIC_COLOR256(208), TIC_BLACK, TIC_UNDERLINE);
tic_style c = tic_style_make(TIC_RGB(255, 128, 0), TIC_DEFAULT, TIC_BOLD | TIC_ITALIC);
```

Use any colour on any terminal. TIC works out what the terminal supports, and converts each colour to the nearest one it can show. It respects the [`NO_COLOR`](https://no-color.org) convention, and you can override the result with `tic_set_color_mode`.

Attributes are `TIC_BOLD`, `TIC_DIM`, `TIC_ITALIC`, `TIC_UNDERLINE`, `TIC_BLINK`, `TIC_REVERSE` and `TIC_STRIKE`. Not every terminal shows all of them.

**Avoid bold on `TIC_BLACK`.** Many terminals, including Windows Terminal by default, show the 16 named colours in their bright version when they're bold. Bright black is grey, so bold black text can look faded.

### Clipping

To stop drawing from spilling out of an area, set a clip rectangle, then put the old one back:

```c
tic_rect old = tic_set_clip(s, panel);
tic_text(s, panel.x, panel.y, tic_plain(), very_long_text);
tic_set_clip(s, old);
```

### Untrusted text

It's safe to draw text from files or the network. Control characters are shown as `�`, so they can't reach the terminal as commands.

### Cursor

The cursor is hidden by default. To show it, call this each frame after `tic_clear`:

```c
tic_show_cursor(s, x, y);
```

## Events

`tic_poll(&event, timeout_ms)` waits for input. A timeout of `-1` waits forever, and `0` returns at once, which suits animation loops. It returns 1 if there's an event, 0 if time ran out, and -1 if the terminal has gone away.

```c
tic_event ev;
if (tic_poll(&ev, -1) < 0) break;

switch (ev.type) {
    case TIC_EVENT_KEY:
        if (ev.key == TIC_KEY_UP) move_up();
        if (ev.key == 's' && (ev.mods & TIC_MOD_CTRL)) save();
        if (ev.key == 0x995) { /* a Bengali letter: text keys hold their code point */ }
        break;
    case TIC_EVENT_MOUSE:
        if (ev.action == TIC_MOUSE_PRESS) click(ev.x, ev.y);
        if (ev.action == TIC_MOUSE_SCROLL_DOWN) scroll_down();
        break;
    case TIC_EVENT_PASTE:
        insert_text(ev.text, ev.text_length); /* only valid until the next tic_poll */
        break;
    case TIC_EVENT_RESIZE:
        break; /* the next draw uses the new size automatically */
    default:
        break;
}
```

- **Keys:** text keys hold their Unicode code point, and everything else is a `TIC_KEY_` value such as `TIC_KEY_ENTER` or `TIC_KEY_F1`. If a key isn't doing what you expect, run `build/keys` to see what TIC receives.
- **Mouse:** reporting is off by default, because it stops users selecting text with the mouse. Turn it on with `tic_set_mouse(TIC_MOUSE_BUTTONS)`.
- **Ctrl+Z:** this arrives as a key. To suspend the way terminal apps normally do, call `tic_suspend()` when you see it. It does nothing on Windows.

## Layout

Split a rectangle into rows or columns, and nest the results:

```c
int row_sizes[] = { TIC_FIXED(1), TIC_FILL(1), TIC_FIXED(1) };
int column_sizes[] = { TIC_PERCENT(30), TIC_FILL(1) };
tic_rect rows[3], columns[2];

tic_split_rows(tic_bounds(screen), 0, row_sizes, 3, rows);
tic_split_columns(rows[1], 0, column_sizes, 2, columns);
/* rows[0] is a header, columns[0] a sidebar, columns[1] the main area, rows[2] a footer */
```

| Size | Meaning |
|---|---|
| `TIC_FIXED(n)` | Exactly `n` cells |
| `TIC_PERCENT(p)` | `p` percent of the space |
| `TIC_FILL(w)` | A share of what's left, in proportion to `w` |

The second argument adds that many empty cells between children. If there isn't enough room, later children get less, down to nothing.

## Widgets

Widgets are immediate-mode: there are no widget objects. Each frame, you call a function that draws the widget, handles the current event, and tells you what happened. Your app owns the data, and there's no second copy to keep in sync.

```c
#define TIC_IMPLEMENTATION
#include "tic.h"

#include <stdio.h>
#include <string.h>

static const char *const fruit[] = { "Apple", "Banana", "Cherry", "Date" };

int main(void) {
    /* Widget state has to stay in one place between frames. */
    static tic_ui ui;
    static char name_text[64];
    static tic_input name;
    static tic_list list;
    tic_event ev;

    if (tic_init() != 0) {
        fprintf(stderr, "tic_init failed: %s\n", tic_error());
        return 1;
    }
    tic_set_mouse(TIC_MOUSE_BUTTONS);
    tic_ui_init(&ui);
    tic_input_init(&name, name_text, sizeof name_text);
    name.placeholder = "Your name";

    memset(&ev, 0, sizeof ev);
    for (;;) {
        tic_buffer *s = tic_screen();
        int sizes[] = { TIC_FIXED(1), TIC_FILL(1) };
        tic_rect rows[2];
        tic_split_rows(tic_bounds(s), 1, sizes, 2, rows);

        tic_clear(s);
        tic_ui_begin(&ui, s, &ev);

        if (tic_ui_input(&ui, rows[0], &name) & TIC_UI_SUBMITTED) {
            tic_ui_focus(&ui, &list); /* Enter moves on to the list */
        }
        if (tic_ui_list(&ui, rows[1], &list, fruit, 4) & TIC_UI_SUBMITTED) {
            break; /* Enter on a row finishes */
        }

        tic_ui_end(&ui);
        tic_present();

        if (tic_poll(&ev, -1) < 0) break;
        if (ev.type == TIC_EVENT_KEY && ev.key == 'q' && (ev.mods & TIC_MOD_CTRL)) break;
    }

    tic_shutdown();
    printf("%s picked %s\n", name_text, fruit[list.selected]);
    return 0;
}
```

The rules for widgets:

- **Call widgets between `tic_ui_begin` and `tic_ui_end`.**
- **State must not move between frames**, because a widget is identified by its state's address. Use `static` variables or a long-lived struct, not a local variable in the drawing function.
- **Tab and Shift+Tab** move between widgets, in the order you draw them. Clicking also moves the focus.
- **Keys go to the focused widget.** Use `tic_ui_event_handled(&ui)` to check whether a widget used an event, if you want it for yourself.

### Text input

`tic_input` is one line of editable UTF-8 text in storage you provide. Read `input.text` at any time. To change it, use `tic_input_set`.

| Keys | Action |
|---|---|
| Left, Right, Home, End | Move |
| Ctrl+A, Ctrl+E | Go to the start or end |
| Ctrl+Left, Ctrl+Right, Alt+B, Alt+F | Move by word |
| Backspace, Delete | Delete a character |
| Ctrl+W, Alt+Backspace, Ctrl+Delete | Delete a word |
| Ctrl+U, Ctrl+K | Delete to the start or end |
| Enter | Returns `TIC_UI_SUBMITTED` |

Optional fields: `input.placeholder` is hint text, and `input.mask` draws a character such as `'*'` in place of the text, for passwords.

### List

`tic_ui_list` draws an array of strings. A zeroed `tic_list` is ready to use, and `list.selected` is the selected row. It returns `TIC_UI_CHANGED` when the selection moves, and `TIC_UI_SUBMITTED` on Enter.

For huge or computed lists, use `tic_ui_list_fn`. It asks for each row's text only when that row is visible, so a million rows cost the same as ten.

### Scrollable view

Pass the full size of your content, then draw it as if it all fits, relative to the rectangle you get back:

```c
static tic_scroll scroll;

tic_rect c = tic_ui_scroll_begin(&ui, area, &scroll, 0, line_count);
for (int i = 0; i < line_count; i++) {
    tic_text(s, c.x, c.y + i, tic_plain(), lines[i]);
}
tic_ui_scroll_end(&ui, &scroll);
```

A `content_width` of 0 means "as wide as the view". The arrows, PageUp and PageDown, Home and End, the mouse wheel and the scrollbar all work. For long content, skip lines that aren't visible and it stays fast.

### Theme

Colours live in `ui.theme`. The defaults use only attributes, so they work with no colour support:

```c
ui.theme.list_selected = tic_style_make(TIC_BLACK, TIC_BRIGHT_CYAN, 0);
```

## Options

Define these before including the header. Full details are at the top of `tic.h`.

| Macro | Where | Effect |
|---|---|---|
| `TIC_NO_UI` | Every file | Leaves out layout and widgets |
| `TIC_MAX_MARKS` | Every file | Combining marks per character (default 3) |
| `TIC_UI_MAX_WIDGETS` | Every file | Focusable widgets per frame (default 128) |
| `TIC_STATIC` | One file | Makes everything private to that file. Only use this if all TIC calls are in that one file |
| `TIC_MALLOC`, `TIC_REALLOC`, `TIC_FREE` | One file | Your own allocator (define all three) |
| `TIC_ESC_TIMEOUT_MS` | One file | How long a lone ESC waits to be the Esc key (default 50) |

## Platform support

The code targets Linux, macOS, the BSDs and Windows 10 or newer, on any terminal that understands standard escape sequences. That covers almost all modern terminals, including Windows Terminal and the Windows 10 console.

**What has actually been tested so far:**

| Platform | Status |
|---|---|
| Linux (gcc, clang, g++, sanitizers) | All tests pass, locally and in CI, including in real pseudo-terminals |
| Windows (MinGW) | All tests pass in CI, including in a real console through ConPTY |
| Windows (MSVC, as C and C++) | All tests pass in CI, including in a real console through ConPTY |
| macOS (clang) | All tests pass in CI, including in real pseudo-terminals. The test using real `bash` is skipped if the installed version is older than 4.4 |
| BSDs | Not tested, and not in CI |

The tutorial's system monitor has been run by hand on real Windows (gcc, PowerShell, Windows Terminal), and its display looked right. Mouse input and resizing weren't confirmed in that run. Nobody has yet looked at the output on a real macOS screen. The tests check what the program sends and receives, not how it looks. The CI workflow is in `.github/workflows/ci.yml`.

## Limitations

- **Complex scripts depend on your terminal.** TIC stores combining marks correctly and keeps the layout from drifting, but how Bengali, Arabic or similar text looks is up to the terminal. Many terminals don't shape these scripts well.
- **No full grapheme clusters.** Emoji joined with a zero-width joiner (such as family emoji) are treated as separate characters, and might look wider than the terminal draws them.
- **Right-to-left text** isn't reordered.
- **Widgets:** there's only text input, list and scrollable view so far. The wheel always scrolls the outermost scroll view.
- **Windows:** mintty (the default window for Git Bash and MSYS2) needs `winpty` in front of the command, because it isn't a real console. Mouse input relies on the console's escape sequence support.

## Troubleshooting

**My terminal is stuck after a crash.** TIC handles almost every case, but not `kill -9` or a stack overflow. Type `reset` and press Enter. If Enter doesn't work, type `stty sane` and press Ctrl+J.

**A key doesn't work.** Run `build/keys` to see what's really arriving. Terminals and multiplexers sometimes capture keys before your app sees them.

**Characters are misaligned.** Your terminal and TIC disagree on a character's width, which is most common with emoji. `build/dashboard` is a quick way to check.

---

# Part 2: Developing TIC

## Layout of the repository

| Path | Contents |
|---|---|
| `tic.h` | The whole library. There's a map of its sections at the top |
| `reference.md` | The API reference: every function, type and option |
| `demo/` | Example programs, which also serve as manual tests |
| `tutorial/` | Tutorials, with a tested program for each step |
| `tests/test_*.c` | Unit tests, which need no terminal |
| `tests/vt.h` | A small terminal emulator, used to check the renderer's output |
| `tests/e2e.py`, `tests/e2e_app.c` | End-to-end tests in a real pseudo-terminal (POSIX) |
| `tests/e2e_conpty.c` | The same idea for Windows, using ConPTY |
| `tests/link/` | Checks each documented way of including the header |
| `tests/check_readme.py` | Compiles this README's examples and checks the names it uses |
| `tests/check_reference.py` | Checks that the API reference matches `tic.h` |
| `tests/check_tutorial.py` | Checks that the code shown in tutorials matches their tested programs |
| `tests/e2e_tutorial.py` | Runs each tutorial step in a real pseudo-terminal |
| `tools/gen_width.py` | Generates the Unicode width tables |
| `.github/workflows/ci.yml` | Runs everything on Linux, macOS and Windows |

## How it works

Output and input are both pipelines, with the OS-specific code only at the very end:

```
OUTPUT   app draws -> back buffer -> RENDERER (compares with front buffer) -> bytes -> PLATFORM -> terminal
INPUT    terminal -> PLATFORM -> bytes -> INPUT PARSER -> events -> tic_poll -> app
```

The design rests on a few decisions. They're worth keeping, and each has a reason:

**The renderer and parser don't touch a terminal.** The renderer only appends to a string, and the parser only takes bytes. That's why nearly everything can be tested on any machine, with no terminal, under sanitizers, and with random input.

**Escape sequences only, on every OS.** Windows uses the same output sequences as Unix, and its input is converted into the same byte stream, so there's only one renderer and one parser. The cost is needing Windows 10 or newer. Don't add a code path based on the old Windows console API, because it would need its own renderer and parser.

**No terminfo.** Nearly every terminal in use today understands the same sequences, and depending on the terminfo database would be a big cost for little gain.

**Wide characters are always kept whole.** A wide character is a lead cell plus a continuation cell (`ch == 0`). Overwriting either half blanks the other. The renderer depends on this, and it's the reason for `tic__split_wide`.

**Only the renderer's cursor tracking trusts a character's width.** Terminals disagree about widths, so `tic__width_is_disputed` makes the renderer re-position after risky characters. That limits a disagreement to one cell, not a whole row.

**The parser has no clock.** Telling the Esc key from the start of a sequence takes a timeout, but the parser only has a `flush` argument. `tic_poll` owns the timing, so parser tests can't be flaky.

**Signal handlers only use async-signal-safe calls.** They can interrupt anything, including `malloc`. See the comment above the signal code in `tic.h` before changing it, and don't add `printf`, `malloc` or `strlen` there.

**Focus uses last frame's widget list.** Tab and clicks are resolved in `tic_ui_begin`, using the list from the previous frame, so every widget in a frame agrees on who has the focus. That's why there's no one-frame lag.

## Building and testing

You need a C compiler, `make`, and Python 3 for the end-to-end tests.

| Command | What it does |
|---|---|
| `make` | Builds the demos into `build/` |
| `make test` | Runs the unit tests |
| `make test-all` | Unit tests with gcc, clang and C++, sanitizers, and link modes |
| `make e2e` | Runs the tests in a real pseudo-terminal |
| `make check-readme` | Compiles this README's examples and checks its names |
| `make tutorial` | Builds the tutorial's step programs into `build/tutorial/` |
| `make check-reference` | Checks that the API reference matches `tic.h` |
| `make check-tutorial` | Checks that the code in tutorials matches their step programs |
| `make windows` | Cross-compiles for Windows (needs MinGW) |
| `make test-windows` | Runs the Windows unit tests under Wine |
| `make e2e-windows` | Runs the ConPTY test under Wine, from a real terminal |
| `tests\build_msvc.bat` | Builds and runs everything with MSVC, from a Developer Command Prompt |

**Before committing,** run `make test-all e2e check-readme check-reference check-tutorial`. If you've touched anything Windows-related, run `make test-windows` too.

Debian and Ubuntu packages: `gcc-mingw-w64-x86-64` for the cross-compiler, and `wine` to run the result.

### Known Wine differences

Wine is useful, but it isn't Windows. `tests/e2e_conpty.c` skips two checks there, because Wine 10 gets them wrong even in programs that don't use TIC: it drops Ctrl+C, after which console waits stop working, and `ResizePseudoConsole` has no effect. Wine's console also doesn't interpret escape sequences as Windows 10 does, so it can't show how the output looks. Only trust real Windows for those.

### Known macOS differences

Three things in `tests/e2e.py` exist because macOS behaves differently from Linux:

- **Session leaders:** macOS revokes a terminal when the process that owns it exits, so its settings can't be read afterwards. The tests run the app under a small holder process for this reason.
- **`PENDIN`:** the settings comparison forgives this one kernel status bit, and prints a note when it does.
- **Ctrl+Z:** `tests/e2e.py` has its own small job-control shell, so that test doesn't depend on which `bash` is installed.

## Writing tests

Tests are C files that include the implementation, so they can call internal `tic__` functions. See the top of `tests/test.h`.

- **New key or sequence:** add it to `tests/test_parser.c`. The `KEY` macro automatically tries it split at every byte position.
- **Renderer change:** add a case to `tests/test_render.c`, and keep `test_random_frames_always_match` passing. It's the most important renderer test.
- **Widget change:** add to `tests/test_ui.c`. Widgets draw on an off-screen buffer with scripted events. The same approach works for testing your own app's UI.
- **Anything to do with signals, timing or real terminals:** add to `tests/e2e.py`.

## Common changes

### Change the API

Update `reference.md`, the tutorials and the README. `make check-reference` fails if the reference no longer matches `tic.h`. To change a tutorial, edit its step programs and the matching code in its text. `make check-tutorial` fails if they disagree, and `make e2e` runs each step.

### Add a new key

1. Add the name to the `TIC_KEY_` enum. Add it at the end, because the order is part of the API. The list in `demo/keys.c` needs updating to match.
2. Map its sequence in `tic__csi_tilde`, `tic__csi_letter` or `tic__parse_ss3`.
3. If Windows reports it as a virtual key, add it to `tic__win_special_key`.
4. Add a test using the `KEY` macro.

### Add a widget

Follow `tic_ui_list_fn` as a template:

1. Define a state struct where zeros are a valid start. Its address is the widget's identity.
2. Call `tic__ui_register` to take part in focus.
3. Use `tic__ui_keyboard_event` and `tic__ui_mouse_event` to get events, and set `ui->event_used` for those you use.
4. Clamp your state first, because the app may have changed it, or the content may have shrunk.
5. Draw inside a narrowed clip (`tic__ui_narrow_clip`), and restore it afterwards.
6. Add it to the `TIC_NO_UI` block, and write off-screen tests, including for sizes down to 0x0.

### Port to a new OS

Implement the functions listed in the comment at the top of the `PLATFORM LAYER` section: `tic__plat_init`, `_shutdown`, `_wait`, `_write`, `_size`, `_now_ms` and `_suspend`, plus `tic__emergency_restore`. Nothing else in the file depends on the OS. A POSIX-like system probably only needs the feature-test macros at the top of the file adjusted.

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
4. Tag the commit. Users only need `tic.h`, so that one file is the release.

## Licence

MIT. See [LICENSE](LICENSE), which is also at the end of `tic.h` so that the licence travels with the file.
