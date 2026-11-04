/* hello.c - the smallest complete cterm program. Press q to quit. */
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
