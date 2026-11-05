/* step3.c - dividing the screen into panels.
 *
 * Part of the system monitor tutorial: see README.md in this folder. */

#define TIC_IMPLEMENTATION
#include "tic.h"

#include <stdio.h>

static void draw_panel(tic_buffer *screen, tic_rect area, const char *title) {
    tic_style border = tic_style_make(TIC_BRIGHT_BLUE, TIC_DEFAULT, 0);
    tic_box(screen, area, TIC_BORDER_ROUNDED, border);
    tic_textf(screen, area.x + 2, area.y, border, " %s ", title);
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

    draw_panel(screen, top[0], "CPU");
    draw_panel(screen, top[1], "Memory");
    draw_panel(screen, rows[2], "Processes");

    tic_text(screen, 1, rows[3].y, help_style, "q: quit");

    tic_present();
}

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
