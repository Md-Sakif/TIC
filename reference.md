# TIC API reference

Everything an app can use from `tic.h`. New to TIC? Start with the [README](README.md) or the [tutorial](tutorial/system-monitor/README.md). This page is for looking things up.

Coordinates count from 0, and (0, 0) is the top-left cell. Unless an entry says otherwise, call functions from a single thread.

## Contents

- [Setup and options](#setup-and-options)
- [Lifecycle](#lifecycle): [`tic_init`](#tic_init), [`tic_shutdown`](#tic_shutdown), [`tic_error`](#tic_error), [`tic_suspend`](#tic_suspend)
- [Screen](#screen): [`tic_screen`](#tic_screen), [`tic_present`](#tic_present), [`tic_redraw`](#tic_redraw)
- [Drawing](#drawing): [`tic_bounds`](#tic_bounds), [`tic_clear`](#tic_clear), [`tic_put`](#tic_put), [`tic_text`](#tic_text), [`tic_textn`](#tic_textn), [`tic_textf`](#tic_textf), [`tic_fill`](#tic_fill), [`tic_box`](#tic_box), [`tic_hline`](#tic_hline), [`tic_vline`](#tic_vline), [`tic_set_clip`](#tic_set_clip), [`tic_show_cursor`](#tic_show_cursor), [`tic_hide_cursor`](#tic_hide_cursor), [`tic_border`](#tic_border)
- [Colours and styles](#colours-and-styles): [`tic_color`](#tic_color), [`TIC_DEFAULT`](#tic_default), [`TIC_COLOR256`](#tic_color256), [`TIC_RGB`](#tic_rgb), [named colours](#named-colours), [text attributes](#text-attributes), [`tic_style`](#tic_style), [`tic_style_make`](#tic_style_make), [`tic_plain`](#tic_plain), [`tic_color_mode`](#tic_color_mode), [`tic_get_color_mode`](#tic_get_color_mode), [`tic_set_color_mode`](#tic_set_color_mode)
- [Rectangles](#rectangles): [`tic_rect`](#tic_rect), [`tic_rect_make`](#tic_rect_make), [`tic_rect_intersect`](#tic_rect_intersect), [`tic_rect_contains`](#tic_rect_contains), [`tic_rect_inset`](#tic_rect_inset)
- [Cell buffers](#cell-buffers): [`tic_cell`](#tic_cell), [`tic_buffer`](#tic_buffer), [`tic_buffer_new`](#tic_buffer_new), [`tic_buffer_free`](#tic_buffer_free), [`tic_buffer_resize`](#tic_buffer_resize), [`tic_buffer_line`](#tic_buffer_line)
- [Unicode](#unicode): [`tic_char_width`](#tic_char_width), [`tic_text_width`](#tic_text_width)
- [Events](#events): [`tic_poll`](#tic_poll), [`tic_event`](#tic_event), [`tic_event_type`](#tic_event_type), [key codes](#key-codes), [modifier flags](#modifier-flags), [`tic_mouse_action`](#tic_mouse_action), [`tic_mouse_button`](#tic_mouse_button), [`tic_mouse_mode`](#tic_mouse_mode), [`tic_set_mouse`](#tic_set_mouse)
- [Layout](#layout): [`TIC_FIXED`](#tic_fixed), [`TIC_PERCENT`](#tic_percent), [`TIC_FILL`](#tic_fill-1), [`tic_split_rows`](#tic_split_rows), [`tic_split_columns`](#tic_split_columns)
- [Widget basics](#widget-basics): [`tic_ui`](#tic_ui), [`tic_ui_theme`](#tic_ui_theme), [`tic_ui_slot`](#tic_ui_slot), [`tic_ui_init`](#tic_ui_init), [`tic_ui_begin`](#tic_ui_begin), [`tic_ui_end`](#tic_ui_end), [`tic_ui_event_handled`](#tic_ui_event_handled), [`tic_ui_focus`](#tic_ui_focus), [`tic_ui_has_focus`](#tic_ui_has_focus), [widget result flags](#widget-result-flags)
- [Text input](#text-input): [`tic_input`](#tic_input), [`tic_input_init`](#tic_input_init), [`tic_input_set`](#tic_input_set), [`tic_ui_input`](#tic_ui_input)
- [List](#list): [`tic_list`](#tic_list), [`tic_ui_list`](#tic_ui_list), [`tic_ui_list_fn`](#tic_ui_list_fn), [`tic_list_text_fn`](#tic_list_text_fn)
- [Scrollable view](#scrollable-view): [`tic_scroll`](#tic_scroll), [`tic_ui_scroll_begin`](#tic_ui_scroll_begin), [`tic_ui_scroll_end`](#tic_ui_scroll_end)

---

## Setup and options

Define these before including `tic.h`.

### `TIC_IMPLEMENTATION`

```c
#define TIC_IMPLEMENTATION
```

**Use it to** put the library's code into your program. Define it in exactly one source file, and put it and the `#include "tic.h"` line before every other `#include` in that file.

### `TIC_STATIC`

```c
#define TIC_STATIC
```

**Use it to** make every TIC function private to one source file. Define it alongside [`TIC_IMPLEMENTATION`](#tic_implementation). Only use it if all your TIC calls are in that one file, because the terminal is shared by the whole process.

### `TIC_API`

```c
#define TIC_API
```

**Use it to** choose what goes in front of every public function, such as `__declspec(dllexport)` when building a Windows DLL. Define it the same way in every file. It defaults to `extern`, or to `static` with [`TIC_STATIC`](#tic_static).

### `TIC_NO_UI`

```c
#define TIC_NO_UI
```

**Use it to** leave out the [layout](#layout) and widget functions when you only need drawing and events. Define it the same way in every file.

### `TIC_MAX_MARKS`

```c
#define TIC_MAX_MARKS 3
```

**Use it to** change how many combining marks one character can hold. Marks past this limit are dropped. Define it the same way in every file. See [`tic_cell`](#tic_cell).

### `TIC_UI_MAX_WIDGETS`

```c
#define TIC_UI_MAX_WIDGETS 128
```

**Use it to** change how many widgets per frame can take the focus. Extra widgets still draw, but Tab and clicks can't reach them. Define it the same way in every file. See [`tic_ui`](#tic_ui).

### `TIC_MALLOC`

```c
#define TIC_MALLOC(size) malloc(size)
```

**Use it to** supply your own allocator. Define it with [`TIC_REALLOC`](#tic_realloc) and [`TIC_FREE`](#tic_free), all three together, where [`TIC_IMPLEMENTATION`](#tic_implementation) is defined.

- `size`: the number of bytes to allocate.

### `TIC_REALLOC`

```c
#define TIC_REALLOC(ptr, size) realloc(ptr, size)
```

**Use it to** supply your own allocator's resize function. See [`TIC_MALLOC`](#tic_malloc).

- `ptr`: the memory to resize, or `NULL` for new memory.
- `size`: the new size in bytes.

### `TIC_FREE`

```c
#define TIC_FREE(ptr) free(ptr)
```

**Use it to** supply your own allocator's free function. See [`TIC_MALLOC`](#tic_malloc).

- `ptr`: the memory to release. It may be `NULL`.

### `TIC_ESC_TIMEOUT_MS`

```c
#define TIC_ESC_TIMEOUT_MS 50
```

**Use it to** change how many milliseconds a lone ESC byte waits before it counts as the Esc key. Terminals start other keys' sequences with the same byte. Too short can break arrow keys over slow connections, and too long makes Esc feel slow. Define it where [`TIC_IMPLEMENTATION`](#tic_implementation) is defined.

### `TIC_PASTE_TIMEOUT_MS`

```c
#define TIC_PASTE_TIMEOUT_MS 500
```

**Use it to** change how many milliseconds a stalled paste waits before TIC delivers what it has. Define it where [`TIC_IMPLEMENTATION`](#tic_implementation) is defined.

### `TIC_PASTE_MAX`

```c
#define TIC_PASTE_MAX (16u * 1024u * 1024u)
```

**Use it to** change the longest paste TIC keeps, in bytes. Anything past this is dropped. Define it where [`TIC_IMPLEMENTATION`](#tic_implementation) is defined.

---

## Lifecycle

### `tic_init`

```c
int tic_init(void);
```

**Use it to** take over the terminal when your program starts. Call it before any function that uses the screen or events.

**Arguments:** none.

**Returns:** 0 on success, or -1 on failure, with the reason in [`tic_error`](#tic_error). It fails if stdin or stdout isn't a terminal. Calling it again while TIC is running does nothing and returns 0.

**See also:** [`tic_shutdown`](#tic_shutdown).

### `tic_shutdown`

```c
void tic_shutdown(void);
```

**Use it to** give the terminal back when your program finishes. It's safe to call more than once. TIC also restores the terminal at `exit()` and on crashes and fatal signals.

**Arguments:** none.

After this, the pointer from [`tic_screen`](#tic_screen) is no longer valid. You can call [`tic_init`](#tic_init) again.

### `tic_error`

```c
const char *tic_error(void);
```

**Use it to** find out why a call failed, usually to show the user.

**Arguments:** none.

**Returns:** a description of the last failure, or `""` if nothing has failed. It's never `NULL`. Successful calls don't clear it, so only read it after a call reports a failure.

### `tic_suspend`

```c
int tic_suspend(void);
```

**Use it to** make Ctrl+Z work the way it does in other terminal programs. TIC delivers Ctrl+Z as an ordinary key, so call this when you see it. It hands the terminal back to the shell, and returns once the user resumes the app with `fg`. [`tic_poll`](#tic_poll) then reports [`TIC_EVENT_RESIZE`](#tic_event_type), so that you redraw.

**Arguments:** none.

**Returns:** 0, or -1 if TIC isn't running or on Windows, which has no such feature.

---

## Screen

### `tic_screen`

```c
tic_buffer *tic_screen(void);
```

**Use it to** get the buffer you draw on to change the screen.

**Arguments:** none.

**Returns:** the screen's [`tic_buffer`](#tic_buffer), or `NULL` before [`tic_init`](#tic_init). The pointer stays valid until [`tic_shutdown`](#tic_shutdown), even across resizes. Don't resize or free it yourself.

### `tic_present`

```c
int tic_present(void);
```

**Use it to** show what you've drawn. Nothing appears on the terminal until you call this. It only sends the cells that changed, so redrawing the whole screen every frame is cheap.

**Arguments:** none.

**Returns:** 0, or -1 on failure, with the reason in [`tic_error`](#tic_error).

### `tic_redraw`

```c
void tic_redraw(void);
```

**Use it to** make the next [`tic_present`](#tic_present) repaint everything, for example if another program has scribbled on the screen. Apps often bind this to Ctrl+L.

**Arguments:** none.

---

## Drawing

Everything here draws on a [`tic_buffer`](#tic_buffer). Drawing off the edge or outside the [clip area](#tic_set_clip) is safe, and is simply ignored.

### `tic_bounds`

```c
tic_rect tic_bounds(const tic_buffer *buffer);
```

**Use it to** get a buffer's whole area, usually as the starting point for a [layout](#layout).

- `buffer`: the buffer to measure. See [`tic_buffer`](#tic_buffer).

**Returns:** a [`tic_rect`](#tic_rect) at (0, 0), the size of the buffer.

### `tic_clear`

```c
void tic_clear(tic_buffer *buffer);
```

**Use it to** start a new frame. It blanks the buffer, removes the clip area, and hides the cursor. Most apps call it first in each frame, then draw everything again.

- `buffer`: the buffer to clear. See [`tic_buffer`](#tic_buffer).

### `tic_put`

```c
void tic_put(tic_buffer *buffer, int x, int y, uint32_t codepoint, tic_style style);
```

**Use it to** draw a single character by its Unicode number, especially one that isn't on your keyboard, such as a block or line character. A combining mark attaches to the character on its left. Control characters are drawn as `�`.

- `buffer`: the buffer to draw on. See [`tic_buffer`](#tic_buffer).
- `x`, `y`: the cell to draw in.
- `codepoint`: the Unicode number of the character, such as `'a'` or `0x2588`.
- `style`: how it looks. See [`tic_style`](#tic_style).

### `tic_text`

```c
int tic_text(tic_buffer *buffer, int x, int y, tic_style style, const char *utf8);
```

**Use it to** draw text. It stays on one line: there's no wrapping, and anything too long is cut off. Invalid bytes and control characters are drawn as `�`, so it's safe to use with untrusted text.

- `buffer`: the buffer to draw on. See [`tic_buffer`](#tic_buffer).
- `x`, `y`: where the text starts.
- `style`: how it looks. See [`tic_style`](#tic_style).
- `utf8`: the text, as a NUL-terminated UTF-8 string.

**Returns:** the x position just after the text, including any part that was cut off, so that calls can be chained:

```c
/* Example */
int x = tic_text(screen, 0, 0, bold, "Name: ");
tic_text(screen, x, 0, tic_plain(), "Sakif");
```

**See also:** [`tic_textn`](#tic_textn), [`tic_textf`](#tic_textf), [`tic_text_width`](#tic_text_width).

### `tic_textn`

```c
int tic_textn(tic_buffer *buffer, int x, int y, tic_style style, const char *utf8, size_t length);
```

**Use it to** draw part of a string, or text that isn't NUL-terminated. Otherwise, it's the same as [`tic_text`](#tic_text).

- `buffer`: the buffer to draw on. See [`tic_buffer`](#tic_buffer).
- `x`, `y`: where the text starts.
- `style`: how it looks. See [`tic_style`](#tic_style).
- `utf8`: the text, as UTF-8.
- `length`: how many bytes of it to draw.

**Returns:** the x position just after the text.

### `tic_textf`

```c
int tic_textf(tic_buffer *buffer, int x, int y, tic_style style, const char *format, ...);
```

**Use it to** draw text with numbers or other values in it, in the same way as `printf`. Otherwise, it's the same as [`tic_text`](#tic_text).

- `buffer`: the buffer to draw on. See [`tic_buffer`](#tic_buffer).
- `x`, `y`: where the text starts.
- `style`: how it looks. See [`tic_style`](#tic_style).
- `format`: a `printf` format string, such as `"%d items"`.
- `...`: the values for the format string.

**Returns:** the x position just after the text.

### `tic_fill`

```c
void tic_fill(tic_buffer *buffer, tic_rect area, uint32_t codepoint, tic_style style);
```

**Use it to** fill an area with one character. Filling with spaces in a coloured style is how you make a coloured bar or background. A wide character is placed every two cells, and a combining mark fills nothing.

- `buffer`: the buffer to draw on. See [`tic_buffer`](#tic_buffer).
- `area`: the area to fill. See [`tic_rect`](#tic_rect).
- `codepoint`: the Unicode number of the character.
- `style`: how it looks. See [`tic_style`](#tic_style).

### `tic_box`

```c
void tic_box(tic_buffer *buffer, tic_rect area, tic_border border, tic_style style);
```

**Use it to** draw the outline of a rectangle, leaving the inside as it was. To draw inside the box, use [`tic_rect_inset`](#tic_rect_inset)`(area, 1)`. A box 1 cell thin is drawn as a line, and a box with no size draws nothing.

- `buffer`: the buffer to draw on. See [`tic_buffer`](#tic_buffer).
- `area`: the outer edge of the box. See [`tic_rect`](#tic_rect).
- `border`: the line style. See [`tic_border`](#tic_border).
- `style`: how it looks. See [`tic_style`](#tic_style).

### `tic_hline`

```c
void tic_hline(tic_buffer *buffer, int x, int y, int length, tic_border border, tic_style style);
```

**Use it to** draw a horizontal line, going right from (x, y).

- `buffer`: the buffer to draw on. See [`tic_buffer`](#tic_buffer).
- `x`, `y`: where the line starts.
- `length`: how many cells long it is.
- `border`: the line style. See [`tic_border`](#tic_border).
- `style`: how it looks. See [`tic_style`](#tic_style).

### `tic_vline`

```c
void tic_vline(tic_buffer *buffer, int x, int y, int length, tic_border border, tic_style style);
```

**Use it to** draw a vertical line, going down from (x, y).

- `buffer`: the buffer to draw on. See [`tic_buffer`](#tic_buffer).
- `x`, `y`: where the line starts.
- `length`: how many cells long it is.
- `border`: the line style. See [`tic_border`](#tic_border).
- `style`: how it looks. See [`tic_style`](#tic_style).

### `tic_set_clip`

```c
tic_rect tic_set_clip(tic_buffer *buffer, tic_rect area);
```

**Use it to** stop drawing from spilling out of an area, such as a panel. Anything drawn outside it is ignored until you change it back. [`tic_clear`](#tic_clear) also removes it.

- `buffer`: the buffer to limit. See [`tic_buffer`](#tic_buffer).
- `area`: the only area that can be drawn on from now on. It can't grow past the buffer. See [`tic_rect`](#tic_rect).

**Returns:** the previous clip area, so you can put it back:

```c
/* Example */
tic_rect old = tic_set_clip(screen, panel);
tic_text(screen, panel.x, panel.y, tic_plain(), "Very long text that might not fit");
tic_set_clip(screen, old);
```

### `tic_show_cursor`

```c
void tic_show_cursor(tic_buffer *buffer, int x, int y);
```

**Use it to** show the terminal's blinking cursor at a cell once the buffer is presented. [`tic_clear`](#tic_clear) hides it, so call this every frame. If the cell is outside the [clip area](#tic_set_clip), the cursor is hidden instead.

- `buffer`: the buffer. See [`tic_buffer`](#tic_buffer).
- `x`, `y`: the cell to put the cursor in.

### `tic_hide_cursor`

```c
void tic_hide_cursor(tic_buffer *buffer);
```

**Use it to** hide the cursor without clearing the buffer.

- `buffer`: the buffer. See [`tic_buffer`](#tic_buffer).

### `tic_border`

```c
typedef enum tic_border { ... } tic_border;
```

**Use it to** choose a line style for [`tic_box`](#tic_box), [`tic_hline`](#tic_hline) and [`tic_vline`](#tic_vline).

| Value | Looks like |
|---|---|
| `TIC_BORDER_SINGLE` | `┌─┐` |
| `TIC_BORDER_DOUBLE` | `╔═╗` |
| `TIC_BORDER_ROUNDED` | `╭─╮` |
| `TIC_BORDER_HEAVY` | `┏━┓` |
| `TIC_BORDER_ASCII` | `+-+`, for fonts without the others |

---

## Colours and styles

### `tic_color`

```c
typedef uint32_t tic_color;
```

**Use it to** store a colour. Make one with [`TIC_DEFAULT`](#tic_default), a [named colour](#named-colours), [`TIC_COLOR256`](#tic_color256) or [`TIC_RGB`](#tic_rgb). You can draw with any colour on any terminal: TIC converts it to the nearest one the terminal can show.

### `TIC_DEFAULT`

```c
#define TIC_DEFAULT
```

**Use it to** ask for whatever colour the terminal normally uses. See [`tic_color`](#tic_color).

### `TIC_COLOR256`

```c
#define TIC_COLOR256(n)
```

**Use it to** pick a colour from the terminal's 256-colour palette.

- `n`: the palette number, from 0 to 255.

**Returns:** a [`tic_color`](#tic_color).

### `TIC_RGB`

```c
#define TIC_RGB(r, g, b)
```

**Use it to** pick an exact colour.

- `r`, `g`, `b`: the amounts of red, green and blue, each from 0 to 255.

**Returns:** a [`tic_color`](#tic_color).

### Named colours

**Use these** for the 16 basic colours. Each one is a [`tic_color`](#tic_color). The exact shades depend on the user's terminal settings.

| Colour | Bright version |
|---|---|
| `TIC_BLACK` | `TIC_BRIGHT_BLACK` (usually grey) |
| `TIC_RED` | `TIC_BRIGHT_RED` |
| `TIC_GREEN` | `TIC_BRIGHT_GREEN` |
| `TIC_YELLOW` | `TIC_BRIGHT_YELLOW` |
| `TIC_BLUE` | `TIC_BRIGHT_BLUE` |
| `TIC_MAGENTA` | `TIC_BRIGHT_MAGENTA` |
| `TIC_CYAN` | `TIC_BRIGHT_CYAN` |
| `TIC_WHITE` | `TIC_BRIGHT_WHITE` |

### Text attributes

**Use these** in the `attrs` of a [`tic_style`](#tic_style). Combine them with `|`, as in `TIC_BOLD | TIC_ITALIC`. Not every terminal shows all of them.

| Value | Meaning |
|---|---|
| `TIC_BOLD` | Bold. Many terminals also show the [named colours](#named-colours) in their bright version when bold, so avoid it on `TIC_BLACK`, which can turn grey |
| `TIC_DIM` | Faint |
| `TIC_ITALIC` | Italic |
| `TIC_UNDERLINE` | Underlined |
| `TIC_BLINK` | Blinking |
| `TIC_REVERSE` | Foreground and background swapped |
| `TIC_STRIKE` | Struck through |

### `tic_style`

```c
typedef struct tic_style {
    tic_color fg, bg;
    uint32_t attrs;
} tic_style;
```

**Use it to** say how text looks. Pass one to any drawing function. Make one with [`tic_style_make`](#tic_style_make) or [`tic_plain`](#tic_plain).

- `fg`: the text colour. See [`tic_color`](#tic_color).
- `bg`: the background colour. See [`tic_color`](#tic_color).
- `attrs`: extras such as bold, or 0 for none. See [text attributes](#text-attributes).

### `tic_style_make`

```c
tic_style tic_style_make(tic_color fg, tic_color bg, uint32_t attrs);
```

**Use it to** build a style, for example `tic_style_make(TIC_RED, TIC_DEFAULT, TIC_BOLD)`.

- `fg`: the text colour. See [`tic_color`](#tic_color).
- `bg`: the background colour. See [`tic_color`](#tic_color).
- `attrs`: extras such as bold, or 0 for none. See [text attributes](#text-attributes).

**Returns:** a [`tic_style`](#tic_style).

### `tic_plain`

```c
tic_style tic_plain(void);
```

**Use it to** draw in the terminal's normal text style.

**Arguments:** none.

**Returns:** a [`tic_style`](#tic_style) with default colours and no attributes.

### `tic_color_mode`

```c
typedef enum tic_color_mode { ... } tic_color_mode;
```

**Use it to** read or set how many colours the terminal can show. See [`tic_get_color_mode`](#tic_get_color_mode) and [`tic_set_color_mode`](#tic_set_color_mode).

| Value | Meaning |
|---|---|
| `TIC_COLORS_NONE` | No colours, only attributes such as bold |
| `TIC_COLORS_16` | The 16 basic colours |
| `TIC_COLORS_256` | The 256-colour palette |
| `TIC_COLORS_RGB` | True colour |

### `tic_get_color_mode`

```c
tic_color_mode tic_get_color_mode(void);
```

**Use it to** find out what TIC detected, or to adjust your design for it. TIC works it out in [`tic_init`](#tic_init) from the environment (`NO_COLOR`, `COLORTERM` and `TERM`), so only call this after that.

**Arguments:** none.

**Returns:** a [`tic_color_mode`](#tic_color_mode).

### `tic_set_color_mode`

```c
void tic_set_color_mode(tic_color_mode mode);
```

**Use it to** override what TIC detected, for example from a `--color` command-line option. Call it after [`tic_init`](#tic_init), which would otherwise replace it. The next [`tic_present`](#tic_present) repaints the screen.

- `mode`: the mode to use. See [`tic_color_mode`](#tic_color_mode).

---

## Rectangles

### `tic_rect`

```c
typedef struct tic_rect {
    int x, y;
    int w, h;
} tic_rect;
```

**Use it to** describe an area of the screen.

- `x`, `y`: the top-left cell.
- `w`, `h`: the width and height in cells.

**See also:** [`tic_rect_make`](#tic_rect_make), [`tic_bounds`](#tic_bounds).

### `tic_rect_make`

```c
tic_rect tic_rect_make(int x, int y, int w, int h);
```

**Use it to** build a rectangle.

- `x`, `y`: the top-left cell.
- `w`, `h`: the width and height. Negative values become 0.

**Returns:** a [`tic_rect`](#tic_rect).

### `tic_rect_intersect`

```c
tic_rect tic_rect_intersect(tic_rect a, tic_rect b);
```

**Use it to** find the area two rectangles share.

- `a`, `b`: the two rectangles. See [`tic_rect`](#tic_rect).

**Returns:** the shared [`tic_rect`](#tic_rect). Its size is 0 if they don't overlap.

### `tic_rect_contains`

```c
int tic_rect_contains(tic_rect r, int x, int y);
```

**Use it to** check whether a cell is inside a rectangle, for example whether a mouse click landed on something.

- `r`: the rectangle. See [`tic_rect`](#tic_rect).
- `x`, `y`: the cell.

**Returns:** 1 if the cell is inside, otherwise 0.

### `tic_rect_inset`

```c
tic_rect tic_rect_inset(tic_rect r, int n);
```

**Use it to** get the area inside a border or margin.

- `r`: the rectangle. See [`tic_rect`](#tic_rect).
- `n`: how many cells to remove from every side.

**Returns:** the smaller [`tic_rect`](#tic_rect). Its size stops at 0.

---

## Cell buffers

The screen is a buffer. You can also make your own, to test drawing code without a terminal.

### `tic_cell`

```c
typedef struct tic_cell {
    uint32_t ch;
    uint32_t marks[TIC_MAX_MARKS];
    tic_style style;
} tic_cell;
```

**Use it to** inspect what's in one cell of a buffer. Most apps never need this.

- `ch`: the character's Unicode number. It's 0 in the right half of a wide character.
- `marks`: combining marks on the character, with 0 meaning unused. See [`TIC_MAX_MARKS`](#tic_max_marks).
- `style`: how it looks. See [`tic_style`](#tic_style).

### `tic_buffer`

```c
typedef struct tic_buffer {
    int width, height;
    tic_cell *cells;
    tic_rect clip;
    int cursor_visible;
    int cursor_x, cursor_y;
} tic_buffer;
```

**Use it to** draw. Get the screen from [`tic_screen`](#tic_screen), or make your own with [`tic_buffer_new`](#tic_buffer_new). Read `width` and `height` freely. To change the other fields, use the drawing functions.

- `width`, `height`: the size in cells.
- `cells`: `width * height` cells, row by row. See [`tic_cell`](#tic_cell).
- `clip`: drawing outside this area is ignored. See [`tic_set_clip`](#tic_set_clip).
- `cursor_visible`, `cursor_x`, `cursor_y`: where the cursor goes. See [`tic_show_cursor`](#tic_show_cursor).

### `tic_buffer_new`

```c
tic_buffer *tic_buffer_new(int width, int height);
```

**Use it to** make an off-screen buffer, usually to test drawing code. Free it with [`tic_buffer_free`](#tic_buffer_free).

- `width`, `height`: the size in cells. Negative values become 0.

**Returns:** a blank [`tic_buffer`](#tic_buffer), or `NULL` if out of memory.

### `tic_buffer_free`

```c
void tic_buffer_free(tic_buffer *buffer);
```

**Use it to** release a buffer from [`tic_buffer_new`](#tic_buffer_new). Don't use it on the screen.

- `buffer`: the buffer to free. It may be `NULL`. See [`tic_buffer`](#tic_buffer).

### `tic_buffer_resize`

```c
int tic_buffer_resize(tic_buffer *buffer, int width, int height);
```

**Use it to** change the size of your own buffer. It blanks the contents, removes the clip area, and hides the cursor. Don't use it on the screen, which TIC resizes for you.

- `buffer`: the buffer to resize. See [`tic_buffer`](#tic_buffer).
- `width`, `height`: the new size. Negative values become 0.

**Returns:** 0, or -1 if out of memory, in which case the buffer is unchanged.

### `tic_buffer_line`

```c
int tic_buffer_line(const tic_buffer *buffer, int y, char *out, int cap);
```

**Use it to** read a row of a buffer as text, which is handy for checking your drawing in tests.

- `buffer`: the buffer to read. See [`tic_buffer`](#tic_buffer).
- `y`: the row.
- `out`: where to store the text, as NUL-terminated UTF-8.
- `cap`: the size of `out` in bytes. Text that doesn't fit is cut off.

**Returns:** the length of the text in bytes, or -1 if `y` is out of range or `cap` is 0 or less.

---

## Unicode

### `tic_char_width`

```c
int tic_char_width(uint32_t codepoint);
```

**Use it to** find out how many cells a character takes up.

- `codepoint`: the character's Unicode number.

**Returns:** 0 for combining marks, 2 for wide characters such as CJK and most emoji, and 1 for everything else.

### `tic_text_width`

```c
int tic_text_width(const char *utf8);
```

**Use it to** measure text before drawing it, for example to centre or right-align it. Bytes aren't a good measure, because many characters take several bytes.

- `utf8`: the text, as a NUL-terminated UTF-8 string.

**Returns:** how many cells it takes up. Invalid bytes count as one cell each, because that's how they're drawn.

---

## Events

### `tic_poll`

```c
int tic_poll(tic_event *event, int timeout_ms);
```

**Use it to** wait for the next key, click, paste or resize. This is the waiting part of every app's main loop.

- `event`: where to store the event. It's cleared first, so after a timeout its type is [`TIC_EVENT_NONE`](#tic_event_type). See [`tic_event`](#tic_event).
- `timeout_ms`: how long to wait. Negative means for as long as it takes, 0 means don't wait, and a positive number is a limit in milliseconds.

**Returns:** 1 if there was an event, 0 if the time ran out, or -1 if the terminal has gone away or TIC isn't running. Your loop should stop on -1.

```c
/* Example */
if (tic_poll(&event, 250) < 0) return;
if (event.type == TIC_EVENT_KEY && event.key == TIC_KEY_UP) {
    /* the Up arrow was pressed */
}
```

### `tic_event`

```c
typedef struct tic_event {
    tic_event_type type;
    int key;
    int mods;
    int x, y;
    tic_mouse_action action;
    tic_mouse_button button;
    int width, height;
    const char *text;
    size_t text_length;
} tic_event;
```

**Use it to** read what happened. Check `type`, then read the fields for that type. Filled in by [`tic_poll`](#tic_poll).

- `type`: what kind of event it is. See [`tic_event_type`](#tic_event_type).
- `key`: for keys, a Unicode number such as `'a'` or `0x995`, or one of the [key codes](#key-codes).
- `mods`: for keys and the mouse, which modifier keys were held. See [modifier flags](#modifier-flags).
- `x`, `y`: for the mouse, the cell under the pointer.
- `action`: for the mouse, what it did. See [`tic_mouse_action`](#tic_mouse_action).
- `button`: for the mouse, which button. See [`tic_mouse_button`](#tic_mouse_button).
- `width`, `height`: for resizes, the new size.
- `text`: for pastes, the text. It's NUL-terminated and may be invalid UTF-8. It's only valid until the next [`tic_poll`](#tic_poll).
- `text_length`: for pastes, the length of `text` in bytes.

### `tic_event_type`

```c
typedef enum tic_event_type { ... } tic_event_type;
```

**Use it to** tell what kind of event a [`tic_event`](#tic_event) is.

| Value | Meaning | Fields to read |
|---|---|---|
| `TIC_EVENT_NONE` | Nothing happened (the wait timed out) | None |
| `TIC_EVENT_KEY` | A key was pressed | `key`, `mods` |
| `TIC_EVENT_MOUSE` | The mouse did something. See [`tic_set_mouse`](#tic_set_mouse) | `action`, `button`, `x`, `y`, `mods` |
| `TIC_EVENT_RESIZE` | The terminal changed size, or the app came back after [`tic_suspend`](#tic_suspend). Draw everything again | `width`, `height` |
| `TIC_EVENT_PASTE` | Text was pasted | `text`, `text_length` |

### Key codes

**Use these** to check `event.key` in a [`tic_event`](#tic_event) for keys that aren't text. Text keys hold their Unicode number instead. Both can be compared directly, as in `event.key == 'q'`.

| Value | Key |
|---|---|
| `TIC_KEY_ENTER` | Enter |
| `TIC_KEY_TAB` | Tab. Shift+Tab is this with [`TIC_MOD_SHIFT`](#modifier-flags) |
| `TIC_KEY_BACKSPACE` | Backspace. Ctrl+H also arrives as this |
| `TIC_KEY_ESC` | Esc. See [`TIC_ESC_TIMEOUT_MS`](#tic_esc_timeout_ms) |
| `TIC_KEY_UP`, `TIC_KEY_DOWN`, `TIC_KEY_LEFT`, `TIC_KEY_RIGHT` | Arrows |
| `TIC_KEY_HOME`, `TIC_KEY_END` | Home, End |
| `TIC_KEY_PAGE_UP`, `TIC_KEY_PAGE_DOWN` | PageUp, PageDown |
| `TIC_KEY_INSERT`, `TIC_KEY_DELETE` | Insert, Delete |
| `TIC_KEY_F1`, `TIC_KEY_F2`, `TIC_KEY_F3`, `TIC_KEY_F4` | F1 to F4 |
| `TIC_KEY_F5`, `TIC_KEY_F6`, `TIC_KEY_F7`, `TIC_KEY_F8` | F5 to F8 |
| `TIC_KEY_F9`, `TIC_KEY_F10`, `TIC_KEY_F11`, `TIC_KEY_F12` | F9 to F12 |

### Modifier flags

**Use these** to check `event.mods` in a [`tic_event`](#tic_event). Combine with `&`, as in `event.mods & TIC_MOD_CTRL`.

| Value | Key held |
|---|---|
| `TIC_MOD_SHIFT` | Shift |
| `TIC_MOD_ALT` | Alt |
| `TIC_MOD_CTRL` | Ctrl |

Ctrl+letter arrives as the small letter with `TIC_MOD_CTRL`. So Ctrl+C is `event.key == 'c'` with that flag, and it doesn't stop your program. Always give your app a way to quit.

### `tic_mouse_action`

```c
typedef enum tic_mouse_action { ... } tic_mouse_action;
```

**Use it to** tell what the mouse did, in `event.action` of a [`tic_event`](#tic_event).

| Value | Meaning |
|---|---|
| `TIC_MOUSE_PRESS` | A button was pressed |
| `TIC_MOUSE_RELEASE` | A button was released |
| `TIC_MOUSE_DRAG` | Moved with a button held |
| `TIC_MOUSE_MOVE` | Moved with no button held. Needs [`TIC_MOUSE_MOTION`](#tic_mouse_mode) |
| `TIC_MOUSE_SCROLL_UP`, `TIC_MOUSE_SCROLL_DOWN` | The wheel turned |
| `TIC_MOUSE_SCROLL_LEFT`, `TIC_MOUSE_SCROLL_RIGHT` | The wheel tilted, on mice and trackpads that can |

### `tic_mouse_button`

```c
typedef enum tic_mouse_button { ... } tic_mouse_button;
```

**Use it to** tell which button was involved, in `event.button` of a [`tic_event`](#tic_event).

| Value | Meaning |
|---|---|
| `TIC_BUTTON_NONE` | No button, as with the wheel or plain movement |
| `TIC_BUTTON_LEFT` | Left button |
| `TIC_BUTTON_MIDDLE` | Middle button |
| `TIC_BUTTON_RIGHT` | Right button |

### `tic_mouse_mode`

```c
typedef enum tic_mouse_mode { ... } tic_mouse_mode;
```

**Use it to** choose what mouse activity to report. See [`tic_set_mouse`](#tic_set_mouse).

| Value | Reports |
|---|---|
| `TIC_MOUSE_OFF` | Nothing. This is the default |
| `TIC_MOUSE_BUTTONS` | Presses, releases, drags and the wheel |
| `TIC_MOUSE_MOTION` | All of that, plus movement with no button held |

### `tic_set_mouse`

```c
void tic_set_mouse(tic_mouse_mode mode);
```

**Use it to** turn on mouse input. It's off by default, because it stops users selecting text with the mouse in most terminals. Call it after [`tic_init`](#tic_init). Before that, it does nothing.

- `mode`: what to report. See [`tic_mouse_mode`](#tic_mouse_mode).

---

## Layout

Not available with [`TIC_NO_UI`](#tic_no_ui).

### `TIC_FIXED`

```c
#define TIC_FIXED(n)
```

**Use it to** give a child of [`tic_split_rows`](#tic_split_rows) or [`tic_split_columns`](#tic_split_columns) an exact size.

- `n`: the size in cells.

### `TIC_PERCENT`

```c
#define TIC_PERCENT(p)
```

**Use it to** give a child of [`tic_split_rows`](#tic_split_rows) or [`tic_split_columns`](#tic_split_columns) a percentage of the space, not counting gaps.

- `p`: the percentage, from 0 to 100.

### `TIC_FILL`

```c
#define TIC_FILL(w)
```

**Use it to** give a child of [`tic_split_rows`](#tic_split_rows) or [`tic_split_columns`](#tic_split_columns) a share of whatever space is left over.

- `w`: its share, in proportion to the others. Two `TIC_FILL(1)` get half each, and `TIC_FILL(2)` gets twice the space of `TIC_FILL(1)`. Use 1 or more.

### `tic_split_rows`

```c
void tic_split_rows(tic_rect area, int gap, const int *sizes, int count, tic_rect *out);
```

**Use it to** divide an area into rows, one above the other. Split the results again to build any layout. If there isn't enough room, later rows get less, down to nothing.

- `area`: the area to divide. See [`tic_rect`](#tic_rect), and [`tic_bounds`](#tic_bounds) for the whole screen.
- `gap`: how many empty cells to leave between rows.
- `sizes`: a list of `count` sizes, made with [`TIC_FIXED`](#tic_fixed), [`TIC_PERCENT`](#tic_percent) or [`TIC_FILL`](#tic_fill-1).
- `count`: how many rows.
- `out`: a list with room for `count` [`tic_rect`](#tic_rect) values, which this fills in.

```c
/* Example */
int sizes[] = { TIC_FIXED(1), TIC_FILL(1), TIC_FIXED(1) };
tic_rect rows[3];
tic_split_rows(tic_bounds(screen), 0, sizes, 3, rows);
/* rows[0] is a header, rows[1] the main area, and rows[2] a footer */
```

### `tic_split_columns`

```c
void tic_split_columns(tic_rect area, int gap, const int *sizes, int count, tic_rect *out);
```

**Use it to** divide an area into columns, side by side. Otherwise, it's the same as [`tic_split_rows`](#tic_split_rows).

- `area`: the area to divide. See [`tic_rect`](#tic_rect).
- `gap`: how many empty cells to leave between columns.
- `sizes`: a list of `count` sizes, made with [`TIC_FIXED`](#tic_fixed), [`TIC_PERCENT`](#tic_percent) or [`TIC_FILL`](#tic_fill-1).
- `count`: how many columns.
- `out`: a list with room for `count` [`tic_rect`](#tic_rect) values, which this fills in.

---

## Widget basics

Not available with [`TIC_NO_UI`](#tic_no_ui).

Widgets are immediate-mode: there are no widget objects. Each frame, you call a function that draws the widget, handles the current event, and tells you what happened. A widget is identified by the address of its state ([`tic_input`](#tic_input), [`tic_list`](#tic_list) or [`tic_scroll`](#tic_scroll)), so **that state must stay in the same place between frames**. Make it `static`, or part of a long-lived struct.

```c
/* Example */
tic_clear(screen);
tic_ui_begin(&ui, screen, &event);
if (tic_ui_input(&ui, tic_rect_make(0, 0, 20, 1), &name) & TIC_UI_SUBMITTED) {
    /* the user pressed Enter: name.text holds what they typed */
}
tic_ui_end(&ui);
tic_present();
```

### `tic_ui`

```c
typedef struct tic_ui { tic_ui_theme theme; /* and private fields */ } tic_ui;
```

**Use it to** hold everything the UI remembers between frames. Make one, set it up with [`tic_ui_init`](#tic_ui_init), and use it for the life of your app. It's several kilobytes, so `static` is a good home for it.

- `theme`: the styles widgets draw with. Change it whenever you like. See [`tic_ui_theme`](#tic_ui_theme).

Every other field is private.

### `tic_ui_theme`

```c
typedef struct tic_ui_theme {
    tic_style input;
    tic_style input_focused;
    tic_style placeholder;
    tic_style list_item;
    tic_style list_selected;
    tic_style list_selected_idle;
    tic_style scrollbar_track;
    tic_style scrollbar_thumb;
    tic_style scrollbar_thumb_focused;
} tic_ui_theme;
```

**Use it to** change how widgets look, through `ui.theme`. The defaults only use attributes, so they work with no colour support. Each field is a [`tic_style`](#tic_style).

- `input`: a text input.
- `input_focused`: a text input with the focus.
- `placeholder`: the hint text in an empty input.
- `list_item`: a list row.
- `list_selected`: the selected row in a list with the focus.
- `list_selected_idle`: the selected row in a list without the focus.
- `scrollbar_track`: the line a scrollbar runs along.
- `scrollbar_thumb`: the scrollbar's moving part.
- `scrollbar_thumb_focused`: the moving part, when its widget has the focus.

### `tic_ui_slot`

```c
typedef struct tic_ui_slot { const void *id; tic_rect area; } tic_ui_slot;
```

**Don't use this directly.** It's part of the private fields of [`tic_ui`](#tic_ui), and only appears in the header because that struct needs it.

### `tic_ui_init`

```c
void tic_ui_init(tic_ui *ui);
```

**Use it to** set up a [`tic_ui`](#tic_ui) once, before the first frame. It also sets the default [theme](#tic_ui_theme), so change the theme after this.

- `ui`: the UI state to set up. See [`tic_ui`](#tic_ui).

### `tic_ui_begin`

```c
void tic_ui_begin(tic_ui *ui, tic_buffer *buffer, const tic_event *event);
```

**Use it to** start a frame of widgets. Call it after [`tic_clear`](#tic_clear) and before your first widget. Tab and Shift+Tab move the focus here.

- `ui`: the UI state. See [`tic_ui`](#tic_ui).
- `buffer`: the buffer to draw on, usually from [`tic_screen`](#tic_screen). See [`tic_buffer`](#tic_buffer).
- `event`: the event for widgets to handle, from [`tic_poll`](#tic_poll), or `NULL` if there isn't one. See [`tic_event`](#tic_event).

### `tic_ui_end`

```c
void tic_ui_end(tic_ui *ui);
```

**Use it to** finish a frame of widgets. Call it after your last widget, and before [`tic_present`](#tic_present).

- `ui`: the UI state. See [`tic_ui`](#tic_ui).

### `tic_ui_event_handled`

```c
int tic_ui_event_handled(const tic_ui *ui);
```

**Use it to** find out whether a key is still yours to handle. Call it after your widgets have run.

- `ui`: the UI state. See [`tic_ui`](#tic_ui).

**Returns:** 1 if a widget used this frame's event, otherwise 0.

### `tic_ui_focus`

```c
void tic_ui_focus(tic_ui *ui, const void *widget_state);
```

**Use it to** move the focus to a widget yourself, for example to jump to a list after Enter in a search box. Keys go to the widget with the focus. If nothing has it, the first focusable widget drawn takes it.

- `ui`: the UI state. See [`tic_ui`](#tic_ui).
- `widget_state`: the address of the widget's state: a [`tic_input`](#tic_input), [`tic_list`](#tic_list) or [`tic_scroll`](#tic_scroll).

### `tic_ui_has_focus`

```c
int tic_ui_has_focus(const tic_ui *ui, const void *widget_state);
```

**Use it to** check whether a widget has the focus, for example to highlight the panel around it.

- `ui`: the UI state. See [`tic_ui`](#tic_ui).
- `widget_state`: the address of the widget's state: a [`tic_input`](#tic_input), [`tic_list`](#tic_list) or [`tic_scroll`](#tic_scroll).

**Returns:** 1 if it has the focus, otherwise 0.

### Widget result flags

**Use these** to check what [`tic_ui_input`](#tic_ui_input), [`tic_ui_list`](#tic_ui_list) and [`tic_ui_list_fn`](#tic_ui_list_fn) return. Combine with `&`, as in `result & TIC_UI_CHANGED`.

| Value | Meaning |
|---|---|
| `TIC_UI_CHANGED` | The text or selection changed |
| `TIC_UI_SUBMITTED` | The user pressed Enter |

---

## Text input

Not available with [`TIC_NO_UI`](#tic_no_ui).

### `tic_input`

```c
typedef struct tic_input {
    char *text;
    size_t capacity;
    size_t cursor;
    int scroll;
    const char *placeholder;
    uint32_t mask;
} tic_input;
```

**Use it to** hold the state of a one-line text field. Set it up with [`tic_input_init`](#tic_input_init), and keep it in one place between frames.

- `text`: your storage. It's an ordinary NUL-terminated UTF-8 string that you can read at any time.
- `capacity`: the size of that storage in bytes, including the NUL.
- `cursor`: the cursor's position, as a byte offset into `text`.
- `scroll`: how many cells of text are hidden off the left edge. The widget manages this.
- `placeholder`: optional hint text, shown while the field is empty.
- `mask`: optional character to draw in place of the text, such as `'*'` for a password.

### `tic_input_init`

```c
void tic_input_init(tic_input *input, char *storage, size_t capacity);
```

**Use it to** set up a text field once, before the first frame. To start with some text in it, call [`tic_input_set`](#tic_input_set) afterwards.

- `input`: the state to set up. See [`tic_input`](#tic_input).
- `storage`: where to keep the text. It gets emptied, and has to stay in place as long as the input is used.
- `capacity`: the size of `storage` in bytes, including the NUL.

### `tic_input_set`

```c
void tic_input_set(tic_input *input, const char *utf8);
```

**Use it to** replace the text from your code. The cursor moves to the end. Text that doesn't fit is cut off, line breaks and tabs become spaces, and other control characters are removed.

- `input`: the field to change. See [`tic_input`](#tic_input).
- `utf8`: the new text, as a NUL-terminated UTF-8 string.

### `tic_ui_input`

```c
int tic_ui_input(tic_ui *ui, tic_rect area, tic_input *input);
```

**Use it to** draw a text field and handle typing, editing and pasting. Call it every frame, between [`tic_ui_begin`](#tic_ui_begin) and [`tic_ui_end`](#tic_ui_end).

- `ui`: the UI state. See [`tic_ui`](#tic_ui).
- `area`: where to draw it. Only the top row is used. See [`tic_rect`](#tic_rect).
- `input`: the field's state. See [`tic_input`](#tic_input).

**Returns:** [widget result flags](#widget-result-flags): `TIC_UI_CHANGED` when the text changed, and `TIC_UI_SUBMITTED` on Enter.

| Keys | Action |
|---|---|
| Left, Right, Home, End | Move |
| Ctrl+A, Ctrl+E | Go to the start or end |
| Ctrl+Left, Ctrl+Right, Alt+B, Alt+F | Move by word |
| Backspace, Delete | Delete a character |
| Ctrl+W, Ctrl+Backspace, Alt+Backspace, Ctrl+Delete | Delete a word |
| Ctrl+U, Ctrl+K | Delete to the start or end |
| Enter | Submit |

---

## List

Not available with [`TIC_NO_UI`](#tic_no_ui).

### `tic_list`

```c
typedef struct tic_list {
    int selected;
    int scroll;
    int last_selected;
    int last_height;
} tic_list;
```

**Use it to** hold the state of a list. A struct of zeros is a valid start, so `static tic_list list;` is all the setup it needs. Keep it in one place between frames.

- `selected`: the selected row. Read it at any time. If you set it, the list scrolls to show it.
- `scroll`: the first visible row.
- `last_selected`, `last_height`: private.

### `tic_ui_list`

```c
int tic_ui_list(tic_ui *ui, tic_rect area, tic_list *list, const char *const *items, int count);
```

**Use it to** draw a scrolling list of strings, with one selected row. Call it every frame, between [`tic_ui_begin`](#tic_ui_begin) and [`tic_ui_end`](#tic_ui_end). A scrollbar appears when the items don't all fit.

- `ui`: the UI state. See [`tic_ui`](#tic_ui).
- `area`: where to draw it. See [`tic_rect`](#tic_rect).
- `list`: the list's state. See [`tic_list`](#tic_list).
- `items`: an array of `count` strings.
- `count`: how many items there are.

**Returns:** [widget result flags](#widget-result-flags): `TIC_UI_CHANGED` when the selection moved, and `TIC_UI_SUBMITTED` when Enter was pressed on a row.

**Keys:** Up, Down, PageUp, PageDown, Home, End and Enter. Clicking selects a row, and the wheel and scrollbar scroll without changing the selection.

**See also:** [`tic_ui_list_fn`](#tic_ui_list_fn), for huge or computed lists.

### `tic_ui_list_fn`

```c
int tic_ui_list_fn(tic_ui *ui, tic_rect area, tic_list *list, int count, tic_list_text_fn text, void *user_data);
```

**Use it to** show a list whose rows don't exist in memory as strings, such as a huge list, or rows you format as they're needed. It only asks for rows that are visible, so a million rows cost the same as ten. Otherwise, it's the same as [`tic_ui_list`](#tic_ui_list).

- `ui`: the UI state. See [`tic_ui`](#tic_ui).
- `area`: where to draw it. See [`tic_rect`](#tic_rect).
- `list`: the list's state. See [`tic_list`](#tic_list).
- `count`: how many items there are.
- `text`: your function that gives the text for a row. See [`tic_list_text_fn`](#tic_list_text_fn).
- `user_data`: anything you like, or `NULL`. It's passed along to your function.

**Returns:** the same as [`tic_ui_list`](#tic_ui_list).

### `tic_list_text_fn`

```c
typedef const char *(*tic_list_text_fn)(int index, void *user_data);
```

**Use it to** write the function you pass to [`tic_ui_list_fn`](#tic_ui_list_fn).

- `index`: the row it's asking about, from 0.
- `user_data`: whatever you passed to [`tic_ui_list_fn`](#tic_ui_list_fn).

**Returns:** the row's text as UTF-8, or `NULL` for a blank row. The string only has to stay valid until the next call, so a `static` buffer inside your function is fine.

---

## Scrollable view

Not available with [`TIC_NO_UI`](#tic_no_ui).

### `tic_scroll`

```c
typedef struct tic_scroll { int x, y; /* and private fields */ } tic_scroll;
```

**Use it to** hold the state of a scrollable view. A struct of zeros is a valid start, so `static tic_scroll scroll;` is all the setup it needs. Keep it in one place between frames.

- `x`, `y`: how far it's scrolled, in cells. Read it at any time. If you set it, the view keeps it within the content.

Every other field is private.

### `tic_ui_scroll_begin`

```c
tic_rect tic_ui_scroll_begin(tic_ui *ui, tic_rect area, tic_scroll *scroll, int content_width, int content_height);
```

**Use it to** show content bigger than the space you have. Draw your content after this call as if it all fits, positioned relative to the returned rectangle, and TIC clips what isn't visible. Finish with [`tic_ui_scroll_end`](#tic_ui_scroll_end). Call both every frame, between [`tic_ui_begin`](#tic_ui_begin) and [`tic_ui_end`](#tic_ui_end).

- `ui`: the UI state. See [`tic_ui`](#tic_ui).
- `area`: where the view goes on screen. See [`tic_rect`](#tic_rect).
- `scroll`: the view's state. See [`tic_scroll`](#tic_scroll).
- `content_width`: the content's full width in cells, or 0 for "as wide as the view".
- `content_height`: the content's full height in cells.

**Returns:** a [`tic_rect`](#tic_rect) with the content's position and size. Its x and y already include the scrolling, so they're often off-screen or negative.

```c
/* Example */
int i;
tic_rect c = tic_ui_scroll_begin(&ui, area, &scroll, 0, 100);
for (i = 0; i < 100; i++) {
    tic_textf(screen, c.x, c.y + i, tic_plain(), "Line %d", i);
}
tic_ui_scroll_end(&ui, &scroll);
```

**Keys:** arrows, PageUp, PageDown, Home and End, when the view has the focus. The view can only take the focus if there's something to scroll. The mouse wheel and the vertical scrollbar also work. Other widgets can go inside, but the wheel always scrolls the outer view.

### `tic_ui_scroll_end`

```c
void tic_ui_scroll_end(tic_ui *ui, tic_scroll *scroll);
```

**Use it to** finish a scrollable view. It restores the clip area and draws the scrollbar.

- `ui`: the UI state. See [`tic_ui`](#tic_ui).
- `scroll`: the same state you passed to [`tic_ui_scroll_begin`](#tic_ui_scroll_begin). See [`tic_scroll`](#tic_scroll).
