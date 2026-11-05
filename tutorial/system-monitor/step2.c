/* step2.c - a title bar and a help line, drawn in their own function.
 *
 * Part of the system monitor tutorial: see README.md in this folder. */

#define TIC_IMPLEMENTATION
#include "tic.h"

#include <stdio.h>

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
