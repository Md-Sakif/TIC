/* e2e_app.c - the program tests/e2e.py runs inside a real pseudo-terminal.
 *
 * It writes every event, one per line, to the log file named on the command line,
 * so the test script can check what TIC reported without having to read the
 * screen. Keys: Ctrl+Q quits, Ctrl+P presents a test frame, Ctrl+S suspends,
 * Ctrl+G crashes on purpose, and Ctrl+E calls exit() without tic_shutdown(). */
#define TIC_IMPLEMENTATION
#include "tic.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    FILE *log_file;
    int timeout = -1;

    if (argc < 2) return 2;
    log_file = fopen(argv[1], "w");
    if (!log_file) return 2;
    if (argc >= 3) timeout = atoi(argv[2]);

    if (tic_init() != 0) {
        fprintf(log_file, "init-failed %s\n", tic_error());
        fclose(log_file);
        return 3;
    }
    tic_set_mouse(TIC_MOUSE_BUTTONS);
    fprintf(log_file, "ready %d %d\n", tic_screen()->width, tic_screen()->height);
    fflush(log_file);

    for (;;) {
        tic_event ev;
        int got = tic_poll(&ev, timeout);
        if (got < 0) {
            fprintf(log_file, "closed\n");
            break;
        }
        if (got == 0) {
            fprintf(log_file, "timeout\n");
            fflush(log_file);
            continue;
        }
        switch (ev.type) {
            case TIC_EVENT_KEY:
                fprintf(log_file, "key %d %d\n", ev.key, ev.mods);
                if (ev.mods & TIC_MOD_CTRL) {
                    if (ev.key == 'q') goto done;
                    if (ev.key == 'p') {
                        tic_buffer *s = tic_screen();
                        tic_clear(s);
                        tic_text(s, 1, 1, tic_style_make(TIC_RED, TIC_DEFAULT, TIC_BOLD), "frame-marker");
                        tic_present();
                    }
                    if (ev.key == 's') {
                        fprintf(log_file, "suspend %d\n", tic_suspend());
                    }
                    if (ev.key == 'e') {
                        fflush(log_file);
                        exit(7);
                    }
                    if (ev.key == 'g') {
                        fflush(log_file);
                        abort();
                    }
                }
                break;
            case TIC_EVENT_MOUSE:
                fprintf(log_file, "mouse %d %d %d %d\n", (int)ev.action, (int)ev.button, ev.x, ev.y);
                break;
            case TIC_EVENT_RESIZE:
                fprintf(log_file, "resize %d %d\n", ev.width, ev.height);
                break;
            case TIC_EVENT_PASTE:
                fprintf(log_file, "paste %s\n", ev.text);
                break;
            default:
                break;
        }
        fflush(log_file);
    }
done:
    tic_shutdown();
    fprintf(log_file, "done\n");
    fclose(log_file);
    return 0;
}
