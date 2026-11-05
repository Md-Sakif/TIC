/* hello.c - the smallest complete TIC program. Press q to quit. */
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
