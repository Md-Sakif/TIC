/* keys.c - shows the event TIC reports for every key, click and paste.
 *
 * Useful when a key doesn't behave as you expect, and as a minimal example of an
 * event loop. Press Ctrl+Q to quit. */
#define TIC_IMPLEMENTATION
#include "tic.h"

#include <stdio.h>

#define HISTORY 200

static char history[HISTORY][128];
static int history_count = 0;

static void add_line(const char *text) {
    int slot = history_count % HISTORY;
    snprintf(history[slot], sizeof history[slot], "%s", text);
    history_count++;
}

static const char *special_key_name(int key) {
    static const char *const names[] = {
        "Enter", "Tab", "Backspace", "Esc", "Up", "Down", "Left", "Right", "Home", "End",
        "PageUp", "PageDown", "Insert", "Delete", "F1", "F2", "F3", "F4", "F5", "F6", "F7",
        "F8", "F9", "F10", "F11", "F12"
    };
    return key >= TIC_KEY_ENTER && key <= TIC_KEY_F12 ? names[key - TIC_KEY_ENTER] : NULL;
}

static const char *mods_text(int mods) {
    static char text[32];
    snprintf(text, sizeof text, "%s%s%s", mods & TIC_MOD_CTRL ? "Ctrl+" : "",
             mods & TIC_MOD_ALT ? "Alt+" : "", mods & TIC_MOD_SHIFT ? "Shift+" : "");
    return text;
}

static void describe(const tic_event *ev) {
    char line[128];
    switch (ev->type) {
        case TIC_EVENT_KEY: {
            const char *name = special_key_name(ev->key);
            if (name) snprintf(line, sizeof line, "key    %s%s", mods_text(ev->mods), name);
            else snprintf(line, sizeof line, "key    %sU+%04X", mods_text(ev->mods), (unsigned)ev->key);
            break;
        }
        case TIC_EVENT_MOUSE: {
            static const char *const actions[] = {
                "", "press", "release", "drag", "move", "scroll up", "scroll down", "scroll left", "scroll right"
            };
            snprintf(line, sizeof line, "mouse  %s%s button=%d at %d,%d", mods_text(ev->mods),
                     actions[ev->action], (int)ev->button, ev->x, ev->y);
            break;
        }
        case TIC_EVENT_RESIZE:
            snprintf(line, sizeof line, "resize %dx%d", ev->width, ev->height);
            break;
        case TIC_EVENT_PASTE:
            snprintf(line, sizeof line, "paste  %d bytes", (int)ev->text_length);
            break;
        default:
            return;
    }
    add_line(line);
}

static void draw(void) {
    tic_buffer *screen = tic_screen();
    int rows = screen->height - 2, i;
    tic_clear(screen);
    tic_text(screen, 0, 0, tic_style_make(TIC_DEFAULT, TIC_DEFAULT, TIC_BOLD),
             "Press keys, click, or paste. Ctrl+Q quits.");
    for (i = 0; i < rows && i < history_count && i < HISTORY; i++) {
        int index = history_count - 1 - i;
        tic_text(screen, 0, 2 + i, tic_plain(), history[index % HISTORY]);
    }
    tic_present();
}

int main(void) {
    if (tic_init() != 0) {
        fprintf(stderr, "tic_init failed: %s\n", tic_error());
        return 1;
    }
    tic_set_mouse(TIC_MOUSE_BUTTONS);

    for (;;) {
        tic_event ev;
        draw();
        if (tic_poll(&ev, -1) < 0) break;
        if (ev.type == TIC_EVENT_KEY && ev.key == 'q' && (ev.mods & TIC_MOD_CTRL)) break;
        describe(&ev);
    }

    tic_shutdown();
    return 0;
}
