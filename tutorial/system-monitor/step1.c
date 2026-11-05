/* step1.c - the smallest possible terminal program.
 *
 * Part of the system monitor tutorial: see README.md in this folder. */

#define CTERM_IMPLEMENTATION
#include "cterm.h"

#include <stdio.h>

int main(void) {
    if (cterm_init() != 0) {
        fprintf(stderr, "Could not start: %s\n", cterm_error());
        return 1;
    }

    for (;;) {
        cterm_buffer *screen = cterm_screen();
        cterm_event event;

        cterm_clear(screen);
        cterm_text(screen, 2, 1, cterm_plain(), "My system monitor. Press q to quit.");
        cterm_present();

        if (cterm_poll(&event, -1) < 0) break;
        if (event.type == CTERM_EVENT_KEY && event.key == 'q') break;
    }

    cterm_shutdown();
    return 0;
}
