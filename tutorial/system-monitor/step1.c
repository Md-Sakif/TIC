/* step1.c - the smallest possible terminal program.
 *
 * Part of the system monitor tutorial: see README.md in this folder. */

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
