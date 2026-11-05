/* step3.c - dividing the screen into panels.
 *
 * Part of the system monitor tutorial: see README.md in this folder. */

#define CTERM_IMPLEMENTATION
#include "cterm.h"

#include <stdio.h>

static void draw_panel(cterm_buffer *screen, cterm_rect area, const char *title) {
    cterm_style border = cterm_style_make(CTERM_BRIGHT_BLUE, CTERM_DEFAULT, 0);
    cterm_box(screen, area, CTERM_BORDER_ROUNDED, border);
    cterm_textf(screen, area.x + 2, area.y, border, " %s ", title);
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

    draw_panel(screen, top[0], "CPU");
    draw_panel(screen, top[1], "Memory");
    draw_panel(screen, rows[2], "Processes");

    cterm_text(screen, 1, rows[3].y, help_style, "q: quit");

    cterm_present();
}

int main(void) {
    if (cterm_init() != 0) {
        fprintf(stderr, "Could not start: %s\n", cterm_error());
        return 1;
    }

    for (;;) {
        cterm_event event;

        draw();

        if (cterm_poll(&event, -1) < 0) break;
        if (event.type == CTERM_EVENT_KEY && event.key == 'q') break;
    }

    cterm_shutdown();
    return 0;
}
