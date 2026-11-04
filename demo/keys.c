/* keys.c - shows the event cterm reports for every key, click and paste.
 *
 * Useful when a key doesn't behave as you expect, and as a minimal example of an
 * event loop. Press Ctrl+Q to quit. */
#define CTERM_IMPLEMENTATION
#include "cterm.h"

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
    return key >= CTERM_KEY_ENTER && key <= CTERM_KEY_F12 ? names[key - CTERM_KEY_ENTER] : NULL;
}

static const char *mods_text(int mods) {
    static char text[32];
    snprintf(text, sizeof text, "%s%s%s", mods & CTERM_MOD_CTRL ? "Ctrl+" : "",
             mods & CTERM_MOD_ALT ? "Alt+" : "", mods & CTERM_MOD_SHIFT ? "Shift+" : "");
    return text;
}

static void describe(const cterm_event *ev) {
    char line[128];
    switch (ev->type) {
        case CTERM_EVENT_KEY: {
            const char *name = special_key_name(ev->key);
            if (name) snprintf(line, sizeof line, "key    %s%s", mods_text(ev->mods), name);
            else snprintf(line, sizeof line, "key    %sU+%04X", mods_text(ev->mods), (unsigned)ev->key);
            break;
        }
        case CTERM_EVENT_MOUSE: {
            static const char *const actions[] = {
                "", "press", "release", "drag", "move", "scroll up", "scroll down", "scroll left", "scroll right"
            };
            snprintf(line, sizeof line, "mouse  %s%s button=%d at %d,%d", mods_text(ev->mods),
                     actions[ev->action], (int)ev->button, ev->x, ev->y);
            break;
        }
        case CTERM_EVENT_RESIZE:
            snprintf(line, sizeof line, "resize %dx%d", ev->width, ev->height);
            break;
        case CTERM_EVENT_PASTE:
            snprintf(line, sizeof line, "paste  %d bytes", (int)ev->text_length);
            break;
        default:
            return;
    }
    add_line(line);
}

static void draw(void) {
    cterm_buffer *screen = cterm_screen();
    int rows = screen->height - 2, i;
    cterm_clear(screen);
    cterm_text(screen, 0, 0, cterm_style_make(CTERM_DEFAULT, CTERM_DEFAULT, CTERM_BOLD),
               "Press keys, click, or paste. Ctrl+Q quits.");
    for (i = 0; i < rows && i < history_count && i < HISTORY; i++) {
        int index = history_count - 1 - i;
        cterm_text(screen, 0, 2 + i, cterm_plain(), history[index % HISTORY]);
    }
    cterm_present();
}

int main(void) {
    if (cterm_init() != 0) {
        fprintf(stderr, "cterm_init failed: %s\n", cterm_error());
        return 1;
    }
    cterm_set_mouse(CTERM_MOUSE_BUTTONS);

    for (;;) {
        cterm_event ev;
        draw();
        if (cterm_poll(&ev, -1) < 0) break;
        if (ev.type == CTERM_EVENT_KEY && ev.key == 'q' && (ev.mods & CTERM_MOD_CTRL)) break;
        describe(&ev);
    }

    cterm_shutdown();
    return 0;
}
