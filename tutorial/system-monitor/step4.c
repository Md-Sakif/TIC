/* step4.c - live numbers from the system, updated every second.
 *
 * Part of the system monitor tutorial: see README.md in this folder. */

#define CTERM_IMPLEMENTATION
#include "cterm.h"
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

static void draw_panel(cterm_buffer *screen, cterm_rect area, const char *title) {
    cterm_style border = cterm_style_make(CTERM_BRIGHT_BLUE, CTERM_DEFAULT, 0);
    cterm_box(screen, area, CTERM_BORDER_ROUNDED, border);
    cterm_textf(screen, area.x + 2, area.y, border, " %s ", title);
}

static void draw_cpu(cterm_buffer *screen, cterm_rect area) {
    cterm_rect inside = cterm_rect_inset(area, 1);
    draw_panel(screen, area, "CPU");
    cterm_textf(screen, inside.x, inside.y, cterm_plain(), "Usage: %.1f%%", cpu_percent);
}

static void draw_memory(cterm_buffer *screen, cterm_rect area) {
    cterm_rect inside = cterm_rect_inset(area, 1);
    char used[32], total[32];
    format_bytes(used, sizeof used, memory_used);
    format_bytes(total, sizeof total, memory_total);
    draw_panel(screen, area, "Memory");
    cterm_textf(screen, inside.x, inside.y, cterm_plain(), "%s of %s", used, total);
}

static void draw(void) {
    cterm_buffer *screen = cterm_screen();
    cterm_style title_style = cterm_style_make(CTERM_BLACK, CTERM_CYAN, CTERM_BOLD);
    cterm_style help_style = cterm_style_make(CTERM_BRIGHT_BLACK, CTERM_DEFAULT, 0);

    int row_sizes[] = { CTERM_FIXED(1), CTERM_FIXED(7), CTERM_FILL(1), CTERM_FIXED(1) };
    int top_sizes[] = { CTERM_FILL(2), CTERM_FILL(1) };
    cterm_rect rows[4], top[2];

    cterm_split_rows(cterm_bounds(screen), 0, row_sizes, 4, rows);
    cterm_split_columns(rows[1], 0, top_sizes, 2, top);

    cterm_clear(screen);

    cterm_fill(screen, rows[0], ' ', title_style);
    cterm_text(screen, 1, rows[0].y, title_style, "System Monitor");

    draw_cpu(screen, top[0]);
    draw_memory(screen, top[1]);
    draw_panel(screen, rows[2], "Processes");

    cterm_text(screen, 1, rows[3].y, help_style, "q: quit");

    cterm_present();
}

int main(void) {
    time_t last_update = 0;

    if (cterm_init() != 0) {
        fprintf(stderr, "Could not start: %s\n", cterm_error());
        return 1;
    }

    for (;;) {
        cterm_event event;
        int result;

        /* Get new numbers whenever the clock has moved on to a new second. */
        if (time(NULL) != last_update) {
            update_stats();
            last_update = time(NULL);
        }

        draw();

        /* Wait for a key, but only for a quarter of a second. */
        result = cterm_poll(&event, 250);
        if (result < 0) break;
        if (event.type == CTERM_EVENT_KEY && event.key == 'q') break;
    }

    cterm_shutdown();
    return 0;
}
