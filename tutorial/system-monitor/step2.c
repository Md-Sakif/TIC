/* step2.c - a title bar and a help line, drawn in their own function.
 *
 * Part of the system monitor tutorial: see README.md in this folder. */

#define CTERM_IMPLEMENTATION
#include "cterm.h"

#include <stdio.h>

static void draw(void) {
    cterm_buffer *screen = cterm_screen();
    cterm_style title_style = cterm_style_make(CTERM_BLACK, CTERM_CYAN, CTERM_BOLD);
    cterm_style help_style = cterm_style_make(CTERM_BRIGHT_BLACK, CTERM_DEFAULT, 0);

    cterm_clear(screen);

    /* A coloured bar across the top, with the title on it. */
    cterm_fill(screen, cterm_rect_make(0, 0, screen->width, 1), ' ', title_style);
    cterm_text(screen, 1, 0, title_style, "System Monitor");

    /* A line of help along the bottom. */
    cterm_text(screen, 1, screen->height - 1, help_style, "q: quit");

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
