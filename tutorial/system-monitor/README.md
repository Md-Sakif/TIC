# Build a system monitor

In this tutorial, you'll build a program that shows what your computer is doing, live, right in the terminal:

```
 System Monitor                                                      14:32:07
╭─ CPU ────────────────────────────────────────────╮╭─ Memory ───────────────╮
│ 13.3% ██████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░││███████████░░░░░░░░░░░░░│
│                                                  ││3.2 GB of 8.0 GB        │
│                                                  ││                        │
│                                           ▁      ││                        │
│                                           █▇▅▅▆▅▄││                        │
╰──────────────────────────────────────────────────╯╰────────────────────────╯
╭─ Processes ────────────────────────────────────────────────────────────────╮
│Search: type part of a name                                                 │
│    PID     MEMORY  NAME                                                    │
│   2841     1.2 GB  firefox                                                █│
│   3107     640 MB  code                                                   ││
│   1520     412 MB  Xorg                                                   ││
│   2230     238 MB  gnome-shell                                            ││
│   4411      96 MB  python3                                                ││
│   1203      54 MB  pipewire                                               ││
│   3590      28 MB  bash                                                   ││
│   3944      21 MB  sysmon                                                 ││
│    611      12 MB  systemd                                                ││
╰────────────────────────────────────────────────────────────────────────────╯
 212 processes  Tab: switch box  Up/Down: move  Ctrl+Q: quit
```

You'll build it in 9 small steps, and you'll have something that runs at the end of each one. It works on Linux, macOS and Windows.

**You don't need any experience with terminal programs.** If you've seen a little code in any language, you'll be fine. Anything specific to the C language is explained as it comes up, in notes marked **C note**.

It should take about an hour or two.

## Contents

- [Before you start](#before-you-start)
- [Step 1: Your first terminal program](#step-1-your-first-terminal-program)
- [Step 2: A title bar and a help line](#step-2-a-title-bar-and-a-help-line)
- [Step 3: Divide the screen into panels](#step-3-divide-the-screen-into-panels)
- [Step 4: Show live numbers](#step-4-show-live-numbers)
- [Step 5: Add usage bars](#step-5-add-usage-bars)
- [Step 6: Add a CPU graph](#step-6-add-a-cpu-graph)
- [Step 7: List the running programs](#step-7-list-the-running-programs)
- [Step 8: Add a search box](#step-8-add-a-search-box)
- [Step 9: Finishing touches](#step-9-finishing-touches)
- [Ideas for what to do next](#ideas-for-what-to-do-next)
- [If something goes wrong](#if-something-goes-wrong)
- [How stats.h works](#how-statsh-works)

## Before you start

### Some words you'll need

- **Terminal:** the window where you type commands. It's called Terminal on Linux and macOS. On Windows, the setup below tells you which window to use.
- **Compiler:** a program that turns your code into a program you can run.
- **Cell:** one character's space on the screen. A terminal screen is a grid of cells, like a spreadsheet where each cell holds one character.

### Install a compiler

Open your terminal and follow the instructions for your system.

| System | What to do |
|---|---|
| Ubuntu or Debian Linux | Run `sudo apt install build-essential` |
| Fedora Linux | Run `sudo dnf install gcc` |
| macOS | Run `xcode-select --install` and click through the window that appears |
| Windows | Choose either Visual Studio or MinGW, as described below |

#### Windows: Visual Studio or MinGW?

Either works, and the tutorial's code is the same for both. **Visual Studio** is the bigger download, and is the simplest to set up. **MinGW** is much smaller, and gives you the `gcc` compiler that's also common on Linux.

**To use Visual Studio:**

1. Install [Visual Studio Community](https://visualstudio.microsoft.com/vs/community/), and tick **Desktop development with C++** during setup.
2. Open **Developer Command Prompt for VS** from the Start menu. Use that as your terminal for this tutorial.

**To use MinGW:**

1. Install [MSYS2](https://www.msys2.org), using its default folder, `C:\msys64`.
2. Open **MSYS2 UCRT64** from the Start menu, and run this command to install the compiler:
   ```sh
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc
   ```
3. Close that window. **Don't use it for this tutorial**, because TIC programs can't run in it: it isn't a real Windows console.
4. Open **Command Prompt** from the Start menu. Use that as your terminal for this tutorial. Each time you open it, run this first, so it can find the compiler:
   ```bat
   set PATH=C:\msys64\ucrt64\bin;%PATH%
   ```

### Get the files

Make a folder for your project, and download the two files you need into it. These commands work on all three systems:

```sh
mkdir sysmon
cd sysmon
curl -O https://raw.githubusercontent.com/Md-Sakif/TIC/main/tic.h
curl -O https://raw.githubusercontent.com/Md-Sakif/TIC/main/tutorial/system-monitor/stats.h
```

- **`tic.h`** is the TIC library. It's one file, and there's nothing to install.
- **`stats.h`** is a helper written for this tutorial. It asks your operating system for its CPU, memory and process figures. Every OS does that differently, so this file keeps that mess out of your way.

Create an empty file called `sysmon.c` in the same folder, using any text editor. This is the file you'll work on for the whole tutorial.

### How to build and run your program

You'll use this command after every step. Use the same one every time:

| System | Command |
|---|---|
| Linux or macOS | `cc -std=c99 sysmon.c -o sysmon && ./sysmon` |
| Windows with Visual Studio | `cl /utf-8 sysmon.c && sysmon.exe` |
| Windows with MinGW | `gcc -std=c99 sysmon.c -o sysmon.exe && sysmon.exe` |

### If you get stuck

Each step ends with a link to the complete program at that point, such as [step1.c](step1.c). If yours isn't working and you can't see why, copy that file over your `sysmon.c` and carry on. These files are tested on all three systems, so you can trust them.

## Step 1: Your first terminal program

**Goal:** take over the terminal, show some text, and quit when you press `q`.

Put this in `sysmon.c`:

<!-- file: step1.c -->
```c
#define TIC_IMPLEMENTATION
#include "tic.h"

#include <stdio.h>

int main(void) {
    if (tic_init() != 0) {
        fprintf(stderr, "Could not start: %s\n", tic_error());
        return 1;
    }

    for (;;) {
        tic_buffer *screen = tic_screen();
        tic_event event;

        tic_clear(screen);
        tic_text(screen, 2, 1, tic_plain(), "My system monitor. Press q to quit.");
        tic_present();

        if (tic_poll(&event, -1) < 0) break;
        if (event.type == TIC_EVENT_KEY && event.key == 'q') break;
    }

    tic_shutdown();
    return 0;
}
```

Build and run it with the command from [earlier](#how-to-build-and-run-your-program).

**You should see** your text on an otherwise blank screen. Press `q`, and your terminal comes back exactly as it was.

### What's going on

The two lines at the top bring in TIC. `#define TIC_IMPLEMENTATION` has to come first, before any other `#include`. It tells TIC that this is the file that should contain the library's code.

Then the program does this:

1. **`tic_init()`** takes over the terminal. Normally a terminal waits for you to press Enter and shows what you type. A program like this needs every key straight away, and needs to control the whole screen. If this fails, we print why and stop.
2. **`for (;;)`** repeats forever, until something calls `break`. This loop is the heart of every terminal app.
3. **`tic_screen()`** gets the screen, as something you can draw on.
4. **`tic_clear`** blanks it, and **`tic_text`** writes text starting at column 2, row 1. Both count from 0, so that's the third column and the second row.
5. **`tic_present()`** sends your drawing to the terminal. Nothing appears until you call this.
6. **`tic_poll`** waits for something to happen. The `-1` means "wait as long as it takes". If the terminal has gone away, it returns a negative number, and we stop.
7. If the event was the `q` key, `break` leaves the loop.
8. **`tic_shutdown()`** gives the terminal back.

This is the idea that makes the rest easy: **every time around the loop, redraw the whole screen from scratch.** You never have to work out what changed. TIC compares your drawing with what's already on the screen and only sends the difference, so this is fast.

**C note:** `tic_buffer *screen` means "`screen` points to a buffer". The `*` is how C says "a pointer to", which is a way of referring to something stored elsewhere. `&event` means "the address of `event`", so that `tic_poll` can fill it in for us.

Stuck? Compare with [step1.c](step1.c).

## Step 2: A title bar and a help line

**Goal:** make it look like an app, and start keeping the code tidy.

The drawing code is about to grow a lot, so let's give it its own function. **Replace `main` with these two functions:**

<!-- file: step2.c -->
```c
static void draw(void) {
    tic_buffer *screen = tic_screen();
    tic_style title_style = tic_style_make(TIC_BLACK, TIC_CYAN, TIC_BOLD);
    tic_style help_style = tic_style_make(TIC_BRIGHT_BLACK, TIC_DEFAULT, 0);

    tic_clear(screen);

    /* A coloured bar across the top, with the title on it. */
    tic_fill(screen, tic_rect_make(0, 0, screen->width, 1), ' ', title_style);
    tic_text(screen, 1, 0, title_style, "System Monitor");

    /* A line of help along the bottom. */
    tic_text(screen, 1, screen->height - 1, help_style, "q: quit");

    tic_present();
}
```

<!-- file: step2.c -->
```c
int main(void) {
    if (tic_init() != 0) {
        fprintf(stderr, "Could not start: %s\n", tic_error());
        return 1;
    }

    for (;;) {
        tic_event event;

        draw();

        if (tic_poll(&event, -1) < 0) break;
        if (event.type == TIC_EVENT_KEY && event.key == 'q') break;
    }

    tic_shutdown();
    return 0;
}
```

**You should see** a cyan bar with the title across the top, and grey help text at the bottom. Try resizing your window: both stay in the right place by themselves, because any change of size is an event, and every event causes a redraw.

### What's going on

- **A style** is how text looks: its text colour, its background colour, and extras such as bold. `tic_style_make(TIC_BLACK, TIC_CYAN, TIC_BOLD)` means bold black text on cyan. `TIC_DEFAULT` means "whatever colour the terminal normally uses", and `0` means no extras.
- **A rectangle**, made with `tic_rect_make(x, y, width, height)`, describes an area of the screen. `tic_fill` fills it with a character. Filling with spaces in a coloured style is how you make a coloured bar.
- **`screen->width` and `screen->height`** give the size of the window. The bottom row is `height - 1`, because rows are counted from 0.

**C note:** `screen->width` means "the `width` inside the thing `screen` points to". `static` in front of a function means it's only used in this file. A function must be written above the code that calls it, which is why `draw` goes above `main`.

Stuck? Compare with [step2.c](step2.c).

## Step 3: Divide the screen into panels

**Goal:** split the screen into areas for CPU, memory and processes.

You could work out every position by hand, but it would break whenever the window changes size. Instead, describe the layout, and let TIC do the arithmetic.

**Add this function above `draw`.** It draws a box with a title:

<!-- file: step3.c -->
```c
static void draw_panel(tic_buffer *screen, tic_rect area, const char *title) {
    tic_style border = tic_style_make(TIC_BRIGHT_BLUE, TIC_DEFAULT, 0);
    tic_box(screen, area, TIC_BORDER_ROUNDED, border);
    tic_textf(screen, area.x + 2, area.y, border, " %s ", title);
}
```

**Then replace `draw` with this:**

<!-- file: step3.c -->
```c
static void draw(void) {
    tic_buffer *screen = tic_screen();
    tic_style title_style = tic_style_make(TIC_BLACK, TIC_CYAN, TIC_BOLD);
    tic_style help_style = tic_style_make(TIC_BRIGHT_BLACK, TIC_DEFAULT, 0);

    int row_sizes[] = { TIC_FIXED(1), TIC_FIXED(7), TIC_FILL(1), TIC_FIXED(1) };
    int top_sizes[] = { TIC_FILL(2), TIC_FILL(1) };
    tic_rect rows[4], top[2];

    tic_split_rows(tic_bounds(screen), 0, row_sizes, 4, rows);
    tic_split_columns(rows[1], 0, top_sizes, 2, top);

    tic_clear(screen);

    tic_fill(screen, rows[0], ' ', title_style);
    tic_text(screen, 1, rows[0].y, title_style, "System Monitor");

    draw_panel(screen, top[0], "CPU");
    draw_panel(screen, top[1], "Memory");
    draw_panel(screen, rows[2], "Processes");

    tic_text(screen, 1, rows[3].y, help_style, "q: quit");

    tic_present();
}
```

**You should see:**

```
 System Monitor
╭─ CPU ────────────────────────────────╮╭─ Memory ─────────╮
│                                      ││                  │
│                                      ││                  │
│                                      ││                  │
│                                      ││                  │
│                                      ││                  │
╰──────────────────────────────────────╯╰──────────────────╯
╭─ Processes ──────────────────────────────────────────────╮
│                                                          │
│                                                          │
│                                                          │
╰──────────────────────────────────────────────────────────╯
 q: quit
```

Resize the window, and the panels adjust to fit.

### What's going on

`tic_split_rows` cuts an area into rows. You give it a list of sizes, and it fills in a list of rectangles:

| Row | Size | Used for |
|---|---|---|
| `rows[0]` | `TIC_FIXED(1)`: exactly 1 row | Title bar |
| `rows[1]` | `TIC_FIXED(7)`: exactly 7 rows | CPU and memory |
| `rows[2]` | `TIC_FILL(1)`: all the rows left over | Process list |
| `rows[3]` | `TIC_FIXED(1)`: exactly 1 row | Help line |

Then `tic_split_columns` cuts `rows[1]` into two side by side. `TIC_FILL(2)` and `TIC_FILL(1)` share the space 2 to 1, so CPU gets two thirds. Splitting one piece again like this is how you can build any layout.

**C note:** `int row_sizes[] = { ... }` makes a list (an "array") of numbers. `rows[0]` means the first item in a list, because counting starts from 0. `const char *title` is how C passes a piece of text to a function.

Stuck? Compare with [step3.c](step3.c).

## Step 4: Show live numbers

**Goal:** show real figures that update every second.

So far, the program only redraws when you press a key. A system monitor has to update by itself, so the main loop needs to change.

**Change the top of your file** to bring in `stats.h` and the clock functions:

<!-- file: step4.c -->
```c
#define TIC_IMPLEMENTATION
#include "tic.h"
#include "stats.h"

#include <stdio.h>
#include <time.h>
```

**Right below that, add** some variables to hold the latest figures, and a function that fetches them:

<!-- file: step4.c -->
```c
/* The latest numbers from the system. */
static double cpu_percent = 0.0;
static unsigned long long memory_used = 0, memory_total = 0;
```

<!-- file: step4.c -->
```c
static void update_stats(void) {
    cpu_percent = stats_cpu_percent();
    stats_memory(&memory_used, &memory_total);
}
```

Memory comes back as a count of bytes, and nobody wants to read `8589934592`. **Add this function below `update_stats`** to make it readable:

<!-- file: step4.c -->
```c
/* Writes a number of bytes in a readable way, such as "3.2 GB" or "512 MB". */
static void format_bytes(char *out, size_t size, unsigned long long bytes) {
    double gigabytes = (double)bytes / (1024.0 * 1024.0 * 1024.0);
    if (gigabytes >= 1.0) snprintf(out, size, "%.1f GB", gigabytes);
    else snprintf(out, size, "%llu MB", bytes / (1024 * 1024));
}
```

**Add these two below `draw_panel`.** Each one draws a panel and its contents:

<!-- file: step4.c -->
```c
static void draw_cpu(tic_buffer *screen, tic_rect area) {
    tic_rect inside = tic_rect_inset(area, 1);
    draw_panel(screen, area, "CPU");
    tic_textf(screen, inside.x, inside.y, tic_plain(), "Usage: %.1f%%", cpu_percent);
}
```

<!-- file: step4.c -->
```c
static void draw_memory(tic_buffer *screen, tic_rect area) {
    tic_rect inside = tic_rect_inset(area, 1);
    char used[32], total[32];
    format_bytes(used, sizeof used, memory_used);
    format_bytes(total, sizeof total, memory_total);
    draw_panel(screen, area, "Memory");
    tic_textf(screen, inside.x, inside.y, tic_plain(), "%s of %s", used, total);
}
```

**In `draw`, replace the three `draw_panel` lines with these:**

<!-- file: step4.c -->
```c
    draw_cpu(screen, top[0]);
    draw_memory(screen, top[1]);
    draw_panel(screen, rows[2], "Processes");
```

**Finally, replace `main`:**

<!-- file: step4.c -->
```c
int main(void) {
    time_t last_update = 0;

    if (tic_init() != 0) {
        fprintf(stderr, "Could not start: %s\n", tic_error());
        return 1;
    }

    for (;;) {
        tic_event event;
        int result;

        /* Get new numbers whenever the clock has moved on to a new second. */
        if (time(NULL) != last_update) {
            update_stats();
            last_update = time(NULL);
        }

        draw();

        /* Wait for a key, but only for a quarter of a second. */
        result = tic_poll(&event, 250);
        if (result < 0) break;
        if (event.type == TIC_EVENT_KEY && event.key == 'q') break;
    }

    tic_shutdown();
    return 0;
}
```

**You should see** your CPU usage and memory, updating every second. The CPU shows 0.0% for the first second, because it needs two measurements to compare.

### What's going on

The important change is `tic_poll(&event, 250)`. Before, the `-1` meant "wait forever for a key". Now, it waits at most 250 milliseconds, so the loop keeps going round even if nobody touches the keyboard.

A key press still gets through straight away, because `tic_poll` returns as soon as something happens. So why not wait a full second? That wait isn't lined up with the clock, so it could end just before a new second begins, and the next update would then come almost two seconds after the last one. Checking four times a second keeps the updates on time, and it costs almost nothing, because a redraw with no changes sends nothing to the terminal.

Pressing keys also makes the loop go round faster, which is why the figures aren't simply updated every time around. `time(NULL)` gives the current time in seconds, and comparing it with `last_update` means they refresh once per second, however fast you press keys.

`tic_rect_inset(area, 1)` gives the area inside a panel's border, by shrinking it by 1 on every side.

`tic_textf` works like C's `printf`: `%.1f` is a number with one decimal place, `%s` is a piece of text, and `%%` is a literal percent sign.

**C note:** `double` is a number with decimals. `unsigned long long` is a very large whole number that can't go negative, which is what you need for counting gigabytes in bytes. `(double)bytes` converts it to a decimal, so the division keeps its fractional part. `char used[32]` is space for up to 31 characters of text, and `sizeof used` is its size.

Stuck? Compare with [step4.c](step4.c).

## Step 5: Add usage bars

**Goal:** make it readable at a glance, with bars that change colour.

**Add this function below `draw_panel`:**

<!-- file: step5.c -->
```c
/* Draws a bar like this, where `percent` says how much is filled in:  #######---- */
static void draw_bar(tic_buffer *screen, int x, int y, int width, double percent) {
    int filled = (int)(width * percent / 100.0 + 0.5);
    tic_color color = percent < 60.0 ? TIC_GREEN : percent < 85.0 ? TIC_YELLOW : TIC_RED;

    tic_fill(screen, tic_rect_make(x, y, filled, 1), 0x2588, /* a solid block */
             tic_style_make(color, TIC_DEFAULT, 0));
    tic_fill(screen, tic_rect_make(x + filled, y, width - filled, 1), 0x2591, /* a light block */
             tic_style_make(TIC_BRIGHT_BLACK, TIC_DEFAULT, 0));
}
```

**Replace `draw_cpu` and `draw_memory` with these:**

<!-- file: step5.c -->
```c
static void draw_cpu(tic_buffer *screen, tic_rect area) {
    tic_rect inside = tic_rect_inset(area, 1);
    draw_panel(screen, area, "CPU");
    tic_textf(screen, inside.x, inside.y, tic_plain(), "%5.1f%%", cpu_percent);
    draw_bar(screen, inside.x + 7, inside.y, inside.w - 7, cpu_percent);
}
```

<!-- file: step5.c -->
```c
static void draw_memory(tic_buffer *screen, tic_rect area) {
    tic_rect inside = tic_rect_inset(area, 1);
    double percent = memory_total > 0 ? 100.0 * (double)memory_used / (double)memory_total : 0.0;
    char used[32], total[32];
    format_bytes(used, sizeof used, memory_used);
    format_bytes(total, sizeof total, memory_total);
    draw_panel(screen, area, "Memory");
    draw_bar(screen, inside.x, inside.y, inside.w, percent);
    tic_textf(screen, inside.x, inside.y + 1, tic_plain(), "%s of %s", used, total);
}
```

**You should see** a bar in each panel: green when usage is low, yellow when it's getting high, and red above 85%.

### What's going on

The bar is really two rectangles side by side: solid blocks for the used part, and faint blocks for the rest. To work out how many cells to fill, take the same fraction of the width. For example, 50% of a 40-cell bar is 20 cells. Adding `0.5` before converting to a whole number rounds to the nearest cell.

`0x2588` is the number of a character in Unicode, the worldwide list of characters. It's how you draw a character that isn't on your keyboard. Unicode has a whole set of blocks and lines for drawing, and the graph in the next step uses more of them.

`"%5.1f%%"` means "at least 5 characters wide, with one decimal place". That keeps the bar from shifting sideways when the number goes from `9.9` to `10.0`.

**C note:** `a ? b : c` means "if `a` then `b`, otherwise `c`". The `color` line uses two of them in a row: green below 60, otherwise yellow below 85, otherwise red. The check for `memory_total > 0` avoids dividing by zero.

Stuck? Compare with [step5.c](step5.c).

## Step 6: Add a CPU graph

**Goal:** show how the CPU has been behaving, not only right now.

To draw history, the program needs to remember it. **Replace your variables at the top with these**, which add a list of the last 120 readings:

<!-- file: step6.c -->
```c
/* The latest numbers from the system. */
static double cpu_percent = 0.0;
static unsigned long long memory_used = 0, memory_total = 0;

/* The last two minutes of CPU usage, oldest first. */
#define HISTORY_SIZE 120
static double cpu_history[HISTORY_SIZE];
```

**Replace `update_stats`** so that each reading goes on the end of the list:

<!-- file: step6.c -->
```c
static void update_stats(void) {
    int i;

    cpu_percent = stats_cpu_percent();
    stats_memory(&memory_used, &memory_total);

    /* Slide everything along by one to drop the oldest value, then add the newest. */
    for (i = 0; i < HISTORY_SIZE - 1; i++) cpu_history[i] = cpu_history[i + 1];
    cpu_history[HISTORY_SIZE - 1] = cpu_percent;
}
```

**Add this function below `draw_bar`:**

<!-- file: step6.c -->
```c
/* Draws the history as a graph, with the newest value at the right-hand edge. */
static void draw_graph(tic_buffer *screen, tic_rect area) {
    tic_style style = tic_style_make(TIC_BRIGHT_GREEN, TIC_DEFAULT, 0);
    int column, level;

    for (column = 0; column < area.w && column < HISTORY_SIZE; column++) {
        double value = cpu_history[HISTORY_SIZE - 1 - column];
        int x = area.x + area.w - 1 - column;

        /* Each cell can show 8 heights, so the column's height is counted in eighths. */
        int eighths = (int)(value / 100.0 * area.h * 8.0 + 0.5);

        for (level = 0; level < area.h; level++) {
            int y = area.y + area.h - 1 - level;
            int here = eighths - level * 8;
            if (here >= 8) tic_put(screen, x, y, 0x2588, style);        /* a full block */
            else if (here > 0) tic_put(screen, x, y, 0x2580 + (uint32_t)here, style); /* a partial block */
        }
    }
}
```

**Replace `draw_cpu`** to put the graph under the bar:

<!-- file: step6.c -->
```c
static void draw_cpu(tic_buffer *screen, tic_rect area) {
    tic_rect inside = tic_rect_inset(area, 1);
    tic_rect graph = tic_rect_make(inside.x, inside.y + 1, inside.w, inside.h - 1);
    draw_panel(screen, area, "CPU");
    tic_textf(screen, inside.x, inside.y, tic_plain(), "%5.1f%%", cpu_percent);
    draw_bar(screen, inside.x + 7, inside.y, inside.w - 7, cpu_percent);
    draw_graph(screen, graph);
}
```

**You should see** a small graph growing from the right, one column per second:

```
 System Monitor
╭─ CPU ────────────────────────────────────────────╮╭─ Memory ───────────────╮
│ 12.9% ██████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░││███████████░░░░░░░░░░░░░│
│                                                  ││3.2 GB of 8.0 GB        │
│                                                  ││                        │
│                                                  ││                        │
│                                           ▄▄▅▅▄▄▄││                        │
╰──────────────────────────────────────────────────╯╰────────────────────────╯
```

To see it move, open a big web page, or start something demanding.

### What's going on

The history list always holds 120 readings, with the newest last. Sliding everything along is the simplest way to keep it that way. Since the list starts full of zeros, the graph starts out flat.

The clever part is the height. Unicode has blocks of 8 heights, from `0x2581` (one eighth of a cell) to `0x2588` (a full cell), so `0x2580 + 3` is a three-eighths block. That makes the graph smoother than whole cells would allow.

Each column works from the bottom up. Say a column needs to be 11 eighths tall. The bottom cell has 11 to show, which is more than 8, so it gets a full block. The cell above has 11 - 8 = 3 left, so it gets a three-eighths block. The next has none, so it stays blank.

**C note:** `#define HISTORY_SIZE 120` gives a name to a number, so it's only written in one place. `for (i = 0; i < 10; i++)` counts `i` from 0 to 9. `uint32_t` is a whole number type that TIC uses for characters.

Stuck? Compare with [step6.c](step6.c).

## Step 7: List the running programs

**Goal:** use the big panel for a list of running programs, which you can scroll through.

You could draw this by hand, but then you'd need to handle scrolling, selection and the mouse yourself. TIC has a ready-made list **widget** for that. A widget is a piece of interface that handles its own keys and drawing.

**Change the top of your file:**

<!-- file: step7.c -->
```c
#define TIC_IMPLEMENTATION
#include "tic.h"
#include "stats.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
```

**Add these variables below the others:**

<!-- file: step7.c -->
```c
/* The running programs, biggest memory user first. */
#define MAX_PROCESSES 4096
static stats_process processes[MAX_PROCESSES];
static int process_count = 0;

/* What the widgets remember between frames. */
static tic_ui ui;
static tic_list process_list;
```

**Add this function above `update_stats`**, then **replace `update_stats`:**

<!-- file: step7.c -->
```c
/* Tells qsort how to order two processes: more memory comes first. */
static int compare_by_memory(const void *a, const void *b) {
    const stats_process *first = (const stats_process *)a;
    const stats_process *second = (const stats_process *)b;
    if (first->memory > second->memory) return -1;
    if (first->memory < second->memory) return 1;
    return 0;
}
```

<!-- file: step7.c -->
```c
static void update_stats(void) {
    int i;

    cpu_percent = stats_cpu_percent();
    stats_memory(&memory_used, &memory_total);

    for (i = 0; i < HISTORY_SIZE - 1; i++) cpu_history[i] = cpu_history[i + 1];
    cpu_history[HISTORY_SIZE - 1] = cpu_percent;

    process_count = stats_processes(processes, MAX_PROCESSES);
    qsort(processes, (size_t)process_count, sizeof processes[0], compare_by_memory);
}
```

**Add these two functions below `draw_memory`:**

<!-- file: step7.c -->
```c
/* The list calls this to get the text of each row it shows. */
static const char *process_row(int index, void *user_data) {
    static char row[128];
    char memory[32];
    (void)user_data; /* we don't need this: it says so on purpose, to avoid a warning */
    format_bytes(memory, sizeof memory, processes[index].memory);
    snprintf(row, sizeof row, "%7d  %9s  %s", processes[index].pid, memory, processes[index].name);
    return row;
}
```

<!-- file: step7.c -->
```c
static void draw_processes(tic_buffer *screen, tic_rect area) {
    tic_style heading = tic_style_make(TIC_DEFAULT, TIC_DEFAULT, TIC_BOLD);
    tic_rect inside = tic_rect_inset(area, 1);
    tic_rect rows = tic_rect_make(inside.x, inside.y + 1, inside.w, inside.h - 1);

    draw_panel(screen, area, "Processes");
    tic_textf(screen, inside.x, inside.y, heading, "%7s  %9s  %s", "PID", "MEMORY", "NAME");
    tic_ui_list_fn(&ui, rows, &process_list, process_count, process_row, NULL);
}
```

**Replace `draw`.** It now receives the event, and passes it on to the widgets:

<!-- file: step7.c -->
```c
static void draw(const tic_event *event) {
    tic_buffer *screen = tic_screen();
    tic_style title_style = tic_style_make(TIC_BLACK, TIC_CYAN, TIC_BOLD);
    tic_style help_style = tic_style_make(TIC_BRIGHT_BLACK, TIC_DEFAULT, 0);

    int row_sizes[] = { TIC_FIXED(1), TIC_FIXED(7), TIC_FILL(1), TIC_FIXED(1) };
    int top_sizes[] = { TIC_FILL(2), TIC_FILL(1) };
    tic_rect rows[4], top[2];

    tic_split_rows(tic_bounds(screen), 0, row_sizes, 4, rows);
    tic_split_columns(rows[1], 0, top_sizes, 2, top);

    tic_clear(screen);
    tic_ui_begin(&ui, screen, event);

    tic_fill(screen, rows[0], ' ', title_style);
    tic_text(screen, 1, rows[0].y, title_style, "System Monitor");

    draw_cpu(screen, top[0]);
    draw_memory(screen, top[1]);
    draw_processes(screen, rows[2]);

    tic_text(screen, 1, rows[3].y, help_style, "Up/Down: move  q: quit");

    tic_ui_end(&ui);
    tic_present();
}
```

**And replace `main`:**

<!-- file: step7.c -->
```c
int main(void) {
    time_t last_update = 0;
    tic_event event;

    if (tic_init() != 0) {
        fprintf(stderr, "Could not start: %s\n", tic_error());
        return 1;
    }
    tic_ui_init(&ui);
    memset(&event, 0, sizeof event);

    for (;;) {
        int result;

        if (time(NULL) != last_update) {
            update_stats();
            last_update = time(NULL);
        }

        draw(&event);

        result = tic_poll(&event, 250);
        if (result < 0) break;
        if (event.type == TIC_EVENT_KEY && event.key == 'q') break;
    }

    tic_shutdown();
    return 0;
}
```

**You should see** your running programs, with the biggest memory users at the top. Press the Up and Down arrows, PageUp and PageDown, and Home and End to move around.

### What's going on

**You don't handle the arrow keys anywhere.** The widget does. That's why `draw` now takes the event: `tic_ui_begin` hands it to the widgets, and the list reacts when it sees an arrow key.

Notice the new shape of the loop. An event arrives from `tic_poll`, and the *next* time around, `draw(&event)` hands it to the widgets. That's why `event` now sits outside the loop, and why `memset` clears it before the first time around.

Widgets are "immediate mode": there's no list object to create or destroy. You call `tic_ui_list_fn` every time you draw, and it draws the list as it stands right now. All it remembers is what's in `process_list`: which row is selected, and how far it's scrolled. That's why `ui` and `process_list` are `static` variables. They have to survive from one frame to the next.

The list doesn't take a list of strings. Instead, you hand it a function, `process_row`, and it asks for each row's text when it needs it. It only asks for rows that are on the screen, so a very long list is still fast.

`"%7d  %9s  %s"` lines the columns up: the pid is 7 wide, the memory is 9 wide, and the name takes what's left.

**C note:** `qsort` is C's built-in sort. It can sort anything, because you give it a function saying which of two items comes first. `memset(&event, 0, sizeof event)` fills `event` with zeros, which TIC reads as "no event".

Stuck? Compare with [step7.c](step7.c).

## Step 8: Add a search box

**Goal:** type part of a name to find a program in the list.

There's a catch to plan for. In a search box, `q` has to type the letter q, so it can't quit any more. We'll use Ctrl+Q instead.

**Change the top of your file:**

<!-- file: step8.c -->
```c
#define TIC_IMPLEMENTATION
#include "tic.h"
#include "stats.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
```

**Replace the variables you added in step 7:**

<!-- file: step8.c -->
```c
/* The running programs, biggest memory user first. */
#define MAX_PROCESSES 4096
static stats_process processes[MAX_PROCESSES];
static int process_count = 0;

/* The ones that match the search box. These point into `processes`. */
static const stats_process *matches[MAX_PROCESSES];
static int match_count = 0;

/* What the widgets remember between frames. */
static tic_ui ui;
static tic_list process_list;
static tic_input search;
static char search_text[64];
```

**Add these two functions above `update_stats`:**

<!-- file: step8.c -->
```c
/* Returns 1 if `text` contains `wanted`, treating capital and small letters as equal. */
static int contains(const char *text, const char *wanted) {
    size_t length = strlen(wanted);
    if (length == 0) return 1;
    for (; *text; text++) {
        size_t i;
        for (i = 0; i < length; i++) {
            if (tolower((unsigned char)text[i]) != tolower((unsigned char)wanted[i])) break;
        }
        if (i == length) return 1;
    }
    return 0;
}
```

<!-- file: step8.c -->
```c
/* Rebuilds the list of processes that match what's in the search box. */
static void update_matches(void) {
    int i;
    match_count = 0;
    for (i = 0; i < process_count; i++) {
        if (contains(processes[i].name, search_text)) matches[match_count++] = &processes[i];
    }
}
```

**In `update_stats`, add one line** right after the `qsort` line, so the matches stay up to date:

```c
    update_matches();
```

The list now shows `matches`, not `processes`. **Replace `process_row` and `draw_processes`:**

<!-- file: step8.c -->
```c
/* The list calls this to get the text of each row it shows. */
static const char *process_row(int index, void *user_data) {
    static char row[128];
    char memory[32];
    const stats_process *process = matches[index];
    (void)user_data; /* we don't need this: it says so on purpose, to avoid a warning */
    format_bytes(memory, sizeof memory, process->memory);
    snprintf(row, sizeof row, "%7d  %9s  %s", process->pid, memory, process->name);
    return row;
}
```

<!-- file: step8.c -->
```c
static void draw_processes(tic_buffer *screen, tic_rect area) {
    tic_style heading = tic_style_make(TIC_DEFAULT, TIC_DEFAULT, TIC_BOLD);
    tic_rect inside = tic_rect_inset(area, 1);
    tic_rect search_row = tic_rect_make(inside.x + 8, inside.y, inside.w - 8, 1);
    tic_rect rows = tic_rect_make(inside.x, inside.y + 2, inside.w, inside.h - 2);

    draw_panel(screen, area, "Processes");

    tic_text(screen, inside.x, inside.y, heading, "Search:");
    if (tic_ui_input(&ui, search_row, &search) & TIC_UI_CHANGED) {
        update_matches();
    }

    tic_textf(screen, inside.x, inside.y + 1, heading, "%7s  %9s  %s", "PID", "MEMORY", "NAME");
    tic_ui_list_fn(&ui, rows, &process_list, match_count, process_row, NULL);
}
```

**In `draw`, change the help text** to say this:

<!-- file: step8.c -->
```c
    tic_text(screen, 1, rows[3].y, help_style, "Tab: switch box  Up/Down: move  Ctrl+Q: quit");
```

**And replace `main`:**

<!-- file: step8.c -->
```c
int main(void) {
    time_t last_update = 0;
    tic_event event;

    if (tic_init() != 0) {
        fprintf(stderr, "Could not start: %s\n", tic_error());
        return 1;
    }
    tic_ui_init(&ui);
    tic_input_init(&search, search_text, sizeof search_text);
    search.placeholder = "type part of a name";
    memset(&event, 0, sizeof event);

    for (;;) {
        int result;

        if (time(NULL) != last_update) {
            update_stats();
            last_update = time(NULL);
        }

        draw(&event);

        result = tic_poll(&event, 250);
        if (result < 0) break;
        if (event.type == TIC_EVENT_KEY && event.key == 'q' && (event.mods & TIC_MOD_CTRL)) break;
    }

    tic_shutdown();
    return 0;
}
```

**You should see** a cursor in the search box. Type something, and the list narrows as you type. Press **Tab** to move to the list and use the arrow keys, then Tab again to go back. Press **Ctrl+Q** to quit.

### What's going on

With two widgets, each key has to go to one of them. The one that gets it has the **focus**. TIC gives the focus to the first widget you draw, and Tab moves it to the next, in the order you draw them. You didn't have to write any of that.

`tic_input` gives you a whole text box: typing, arrows, Backspace, Home and End, pasting, and scrolling when the text is too long. The text is stored in `search_text`, an ordinary C string you can read at any time.

The call to `tic_ui_input` tells you what happened, as well as drawing the box. `& TIC_UI_CHANGED` asks "did the text change?". If so, we update the matches straight away, and the list shows the result in this same frame.

`event.mods` says which modifier keys were held down, so `(event.mods & TIC_MOD_CTRL)` means "was Ctrl held?". Without this check, typing a `q` in the search box would quit the program.

**C note:** `matches` is a list of pointers. Rather than copying each matching process, it stores where to find it in `processes`, and that's what `&processes[i]` means. `matches[match_count++] = ...` stores an item, then adds 1 to the count. `for (; *text; text++)` walks along a string until it reaches its end.

Stuck? Compare with [step8.c](step8.c).

## Step 9: Finishing touches

**Goal:** fix the rough edges that separate a demo from a real program.

### Stop the selection jumping around

Select a program in the list and watch: every second, the highlight stays on the same *row*, but the programs move as their memory changes. You end up selecting a different program without touching anything.

The fix is to remember the selected program by its pid, which doesn't change, then find it again afterwards. **Replace `update_stats`:**

<!-- file: step9.c -->
```c
static void update_stats(void) {
    int selected_pid = -1;
    int i;

    /* Remember which process is selected, by its pid, while everything is replaced. */
    if (process_list.selected < match_count) selected_pid = matches[process_list.selected]->pid;

    cpu_percent = stats_cpu_percent();
    stats_memory(&memory_used, &memory_total);

    for (i = 0; i < HISTORY_SIZE - 1; i++) cpu_history[i] = cpu_history[i + 1];
    cpu_history[HISTORY_SIZE - 1] = cpu_percent;

    process_count = stats_processes(processes, MAX_PROCESSES);
    qsort(processes, (size_t)process_count, sizeof processes[0], compare_by_memory);
    update_matches();

    /* Find that process again in its new position. */
    for (i = 0; i < match_count; i++) {
        if (matches[i]->pid == selected_pid) process_list.selected = i;
    }
}
```

### Handle tiny windows, and add a clock

If someone shrinks the window right down, the panels get squashed into nonsense. It's kinder to say so. While we're there, let's put a clock in the title bar. **Replace `draw`:**

<!-- file: step9.c -->
```c
static void draw(const tic_event *event) {
    tic_buffer *screen = tic_screen();
    tic_style title_style = tic_style_make(TIC_BLACK, TIC_CYAN, TIC_BOLD);
    tic_style help_style = tic_style_make(TIC_BRIGHT_BLACK, TIC_DEFAULT, 0);
    char clock_text[16];
    time_t now = time(NULL);

    int row_sizes[] = { TIC_FIXED(1), TIC_FIXED(7), TIC_FILL(1), TIC_FIXED(1) };
    int top_sizes[] = { TIC_FILL(2), TIC_FILL(1) };
    tic_rect rows[4], top[2];

    tic_split_rows(tic_bounds(screen), 0, row_sizes, 4, rows);
    tic_split_columns(rows[1], 0, top_sizes, 2, top);

    tic_clear(screen);

    /* A panel can't show much when the window is tiny, so say so instead. */
    if (screen->width < 50 || screen->height < 15) {
        tic_text(screen, 0, 0, tic_plain(), "Please make this window bigger.");
        tic_present();
        return;
    }

    tic_ui_begin(&ui, screen, event);

    tic_fill(screen, rows[0], ' ', title_style);
    tic_text(screen, 1, rows[0].y, title_style, "System Monitor");
    strftime(clock_text, sizeof clock_text, "%H:%M:%S", localtime(&now));
    tic_text(screen, screen->width - 9, rows[0].y, title_style, clock_text);

    draw_cpu(screen, top[0]);
    draw_memory(screen, top[1]);
    draw_processes(screen, rows[2]);

    tic_textf(screen, 1, rows[3].y, help_style, "%d processes  Tab: switch box  Up/Down: move  Ctrl+Q: quit", process_count);

    tic_ui_end(&ui);
    tic_present();
}
```

### Turn on the mouse and add colour

**In `main`, replace the `tic_ui_init(&ui);` line with these lines:**

<!-- file: step9.c -->
```c
    tic_set_mouse(TIC_MOUSE_BUTTONS);
    tic_ui_init(&ui);
    ui.theme.list_selected = tic_style_make(TIC_BLACK, TIC_BRIGHT_CYAN, 0);
    ui.theme.list_selected_idle = tic_style_make(TIC_BLACK, TIC_WHITE, 0);
```

Mouse reporting is off by default, because it stops people selecting text with the mouse. A list you can click and scroll is worth that trade here.

`ui.theme` holds the styles widgets draw with. `list_selected` is the highlighted row when the list has the focus, and `list_selected_idle` is the highlighted row when it doesn't.

### Add more keys

**In `main`, replace the line that checks for Ctrl+Q with this:**

<!-- file: step9.c -->
```c
        if (event.type == TIC_EVENT_KEY && (event.mods & TIC_MOD_CTRL)) {
            if (event.key == 'q') break;
            if (event.key == 'z') tic_suspend();
            if (event.key == 'l') tic_redraw();
        }
```

- **Ctrl+Z** normally pauses a terminal program, and returns you to the shell. TIC delivers it as an ordinary key, so a program has to do this itself by calling `tic_suspend()`. Type `fg` in your shell to come back. Windows doesn't have this feature, so there it does nothing.
- **Ctrl+L** repaints the whole screen, a common habit in terminal programs. It's useful if another program has scribbled on the screen.

**You should see** the finished monitor. Click on a row. Scroll with the mouse wheel. Drag the scrollbar. Shrink the window until the message appears.

Stuck? Compare with [step9.c](step9.c).

**You've built a real terminal application.** Along the way you've used the same pattern that nearly every TIC program uses: fetch data, draw everything, wait for an event, repeat.

## Ideas for what to do next

Try these in any order. They're roughly easiest first.

1. **Show the number of matches.** Put it in the panel's title, such as `Processes (12)`. Hint: build the title with `snprintf`, like `process_row` does.
2. **Change the colours.** Try `TIC_RGB(255, 128, 0)` for an orange graph. TIC picks the nearest colour on terminals that can't show it.
3. **Make the refresh speed adjustable.** Make F5 and F6 change how often `update_stats` runs. Use function keys, because keys such as `+` and `-` would type into the search box. The key codes are `TIC_KEY_F5` and `TIC_KEY_F6`.
4. **Sort by name or pid.** Write another comparison function for `qsort`, and use a key such as F2 to switch between them.
5. **Show details.** When Enter is pressed on a row, `tic_ui_list_fn` returns `TIC_UI_SUBMITTED`. Show that program's details in a scrollable panel, using `tic_ui_scroll_begin`.
6. **Show each CPU core (Linux).** Add a function to `stats.h` that reads each core's line from `/proc/stat`, and draw a bar for each.

The [main README](../../README.md) describes everything TIC can do, and [`demo/widgets.c`](../../demo/widgets.c) is another complete example.

## If something goes wrong

**`'tic.h' file not found`:** the compiler can't find the file. Make sure `tic.h` and `stats.h` are in the same folder as `sysmon.c`, and that your terminal is in that folder. `ls` (or `dir` on Windows) lists what's there.

**`cc: command not found`, or `'cl' is not recognized`, or `'gcc' is not recognized`:** the compiler isn't installed, or Windows can't find it. With Visual Studio, make sure you're in the Developer Command Prompt. With MinGW, make sure you've run the `set PATH` command in this window. See [Install a compiler](#install-a-compiler).

**`must come before every other #include`:** make sure `#define TIC_IMPLEMENTATION` and `#include "tic.h"` are the first two lines of code in your file.

**`implicit declaration of function`, or `undeclared`:** you're probably using something that's written further down the file, or you've missed an `#include`. In C, things must be written above the code that uses them. Compare the order in your file with that step's complete program.

**`Could not start: stdin and stdout must both be a terminal`:** you ran it with its input or output redirected, or from inside something that isn't a real terminal, such as some editors' Run buttons. Run it directly in a terminal. On Windows, the MSYS2 and Git Bash windows have this problem too. Use Command Prompt or the Developer Command Prompt instead.

**My terminal is behaving strangely after a crash:** TIC repairs it after almost every kind of crash, but if it's still odd, type `reset` and press Enter. You may not see what you're typing. If Enter doesn't work, press Ctrl+J instead.

**The CPU always shows 0.0%:** you may be calling `update_stats` more than once in quick succession, leaving too little time to measure anything. It should run once per second. The very first reading is always 0.

**Lines and blocks look wrong:** your terminal's font is missing those characters. Try a different font, or a different terminal.

**Some programs show `0 MB`:** the OS won't reveal the memory use of some programs, particularly system programs, unless you run as an administrator. That's normal. On macOS this is true of any program you don't own.

## How stats.h works

You don't need to know this to use it, but it's not magic. On Linux, the system shares its figures as ordinary text files in a folder called `/proc`. Try these commands in your terminal:

```sh
head -1 /proc/stat
head -3 /proc/meminfo
cat /proc/self/comm
```

- **CPU:** the first line of `/proc/stat` holds running totals of how long the CPU has spent on each kind of work, since the computer started. `stats_cpu_percent` reads these and compares them with the previous reading. If 100 units of time passed and 25 of them were busy, the CPU was 25% busy. That's why it needs two readings.
- **Memory:** `/proc/meminfo` lists total and available memory, and `stats_memory` subtracts one from the other.
- **Processes:** `/proc` has a numbered folder for each running program. `stats_processes` goes through those folders, and reads each one's name and memory use.

macOS and Windows have system functions for this instead of files, but the ideas are the same. All three versions are in [stats.h](stats.h), with comments, and the whole file is only about 250 lines. If you want to learn how an OS shares this information, that's a good file to read next.
