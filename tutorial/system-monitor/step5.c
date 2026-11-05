/* step5.c - coloured usage bars.
 *
 * Part of the system monitor tutorial: see README.md in this folder. */

#define TIC_IMPLEMENTATION
#include "tic.h"
#include "stats.h"

#include <stdio.h>
#include <time.h>

/* The latest numbers from the system. */
static double cpu_percent = 0.0;
static unsigned long long memory_used = 0, memory_total = 0;

static void update_stats(void) {
    cpu_percent = stats_cpu_percent();
    stats_memory(&memory_used, &memory_total);
}

/* Writes a number of bytes in a readable way, such as "3.2 GB" or "512 MB". */
static void format_bytes(char *out, size_t size, unsigned long long bytes) {
    double gigabytes = (double)bytes / (1024.0 * 1024.0 * 1024.0);
    if (gigabytes >= 1.0) snprintf(out, size, "%.1f GB", gigabytes);
    else snprintf(out, size, "%llu MB", bytes / (1024 * 1024));
}

static void draw_panel(tic_buffer *screen, tic_rect area, const char *title) {
    tic_style border = tic_style_make(TIC_BRIGHT_BLUE, TIC_DEFAULT, 0);
    tic_box(screen, area, TIC_BORDER_ROUNDED, border);
    tic_textf(screen, area.x + 2, area.y, border, " %s ", title);
}

/* Draws a bar like this, where `percent` says how much is filled in:  #######---- */
static void draw_bar(tic_buffer *screen, int x, int y, int width, double percent) {
    int filled = (int)(width * percent / 100.0 + 0.5);
    tic_color color = percent < 60.0 ? TIC_GREEN : percent < 85.0 ? TIC_YELLOW : TIC_RED;

    tic_fill(screen, tic_rect_make(x, y, filled, 1), 0x2588, /* a solid block */
             tic_style_make(color, TIC_DEFAULT, 0));
    tic_fill(screen, tic_rect_make(x + filled, y, width - filled, 1), 0x2591, /* a light block */
             tic_style_make(TIC_BRIGHT_BLACK, TIC_DEFAULT, 0));
}

static void draw_cpu(tic_buffer *screen, tic_rect area) {
    tic_rect inside = tic_rect_inset(area, 1);
    draw_panel(screen, area, "CPU");
    tic_textf(screen, inside.x, inside.y, tic_plain(), "%5.1f%%", cpu_percent);
    draw_bar(screen, inside.x + 7, inside.y, inside.w - 7, cpu_percent);
}

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

static void draw(void) {
    tic_buffer *screen = tic_screen();
    tic_style title_style = tic_style_make(TIC_BLACK, TIC_CYAN, 0);
    tic_style help_style = tic_style_make(TIC_BRIGHT_BLACK, TIC_DEFAULT, 0);

    int row_sizes[] = { TIC_FIXED(1), TIC_FIXED(7), TIC_FILL(1), TIC_FIXED(1) };
    int top_sizes[] = { TIC_FILL(2), TIC_FILL(1) };
    tic_rect rows[4], top[2];

    tic_split_rows(tic_bounds(screen), 0, row_sizes, 4, rows);
    tic_split_columns(rows[1], 0, top_sizes, 2, top);

    tic_clear(screen);

    tic_fill(screen, rows[0], ' ', title_style);
    tic_text(screen, 1, rows[0].y, title_style, "System Monitor");

    draw_cpu(screen, top[0]);
    draw_memory(screen, top[1]);
    draw_panel(screen, rows[2], "Processes");

    tic_text(screen, 1, rows[3].y, help_style, "q: quit");

    tic_present();
}

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
