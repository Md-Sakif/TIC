/* dashboard.c - a static screen showing off the drawing API: boxes, colours, text in
 * several scripts, and clipping. It redraws when the terminal is resized. Press any
 * key to quit. */
#define CTERM_IMPLEMENTATION
#include "cterm.h"

#include <stdio.h>

static void draw_bar(cterm_buffer *s, int x, int y, int width, int percent, cterm_color color) {
    int filled = width * percent / 100;
    cterm_fill(s, cterm_rect_make(x, y, filled, 1), 0x2588, cterm_style_make(color, CTERM_DEFAULT, 0)); /* full block */
    cterm_fill(s, cterm_rect_make(x + filled, y, width - filled, 1), 0x2591,
               cterm_style_make(CTERM_BRIGHT_BLACK, CTERM_DEFAULT, 0)); /* light shade */
    cterm_textf(s, x + width + 1, y, cterm_plain(), "%3d%%", percent);
}

static void draw_panel(cterm_buffer *s, cterm_rect area, const char *title) {
    cterm_box(s, area, CTERM_BORDER_ROUNDED, cterm_style_make(CTERM_BRIGHT_BLUE, CTERM_DEFAULT, 0));
    cterm_textf(s, area.x + 2, area.y, cterm_style_make(CTERM_BRIGHT_WHITE, CTERM_DEFAULT, CTERM_BOLD), " %s ", title);
}

static void draw(void) {
    cterm_buffer *s = cterm_screen();
    int half = s->width / 2;
    cterm_rect left = cterm_rect_make(0, 1, half, s->height - 2);
    cterm_rect right = cterm_rect_make(half, 1, s->width - half, s->height - 2);
    cterm_rect inner;
    cterm_rect old_clip;
    int x, bar_width;

    static const char *const mode_names[] = { "no colour", "16 colours", "256 colours", "true colour" };

    cterm_clear(s);

    /* Header and footer, each a full-width bar. */
    cterm_fill(s, cterm_rect_make(0, 0, s->width, 1), ' ', cterm_style_make(CTERM_BLACK, CTERM_CYAN, 0));
    cterm_text(s, 1, 0, cterm_style_make(CTERM_BLACK, CTERM_CYAN, CTERM_BOLD), "cterm dashboard");
    cterm_textf(s, 1, s->height - 1, cterm_style_make(CTERM_BRIGHT_BLACK, CTERM_DEFAULT, 0),
                "%dx%d, %s. Press any key to quit.", s->width, s->height, mode_names[cterm_get_color_mode()]);

    /* Left: progress bars, and text in several scripts. */
    draw_panel(s, left, "Status");
    inner = cterm_rect_inset(left, 2);
    bar_width = inner.w - 6;
    old_clip = cterm_set_clip(s, inner);
    draw_bar(s, inner.x, inner.y, bar_width, 72, CTERM_GREEN);
    draw_bar(s, inner.x, inner.y + 1, bar_width, 35, CTERM_YELLOW);
    draw_bar(s, inner.x, inner.y + 2, bar_width, 91, CTERM_RED);

    cterm_text(s, inner.x, inner.y + 4, cterm_style_make(CTERM_DEFAULT, CTERM_DEFAULT, CTERM_UNDERLINE), "Text");
    cterm_text(s, inner.x, inner.y + 5, cterm_plain(), "English: Hello");
    cterm_text(s, inner.x, inner.y + 6, cterm_plain(), "\xE0\xA6\xAC\xE0\xA6\xBE\xE0\xA6\x82\xE0\xA6\xB2\xE0\xA6\xBE: \xE0\xA6\xB9\xE0\xA7\x8D\xE0\xA6\xAF\xE0\xA6\xBE\xE0\xA6\xB2\xE0\xA7\x8B");
    cterm_text(s, inner.x, inner.y + 7, cterm_plain(), "\xE4\xB8\xAD\xE6\x96\x87: \xE4\xBD\xA0\xE5\xA5\xBD");
    cterm_text(s, inner.x, inner.y + 8, cterm_plain(), "Emoji: \xF0\x9F\x8E\x89 \xF0\x9F\x9A\x80");
    cterm_set_clip(s, old_clip);

    /* Right: styles and a colour ramp that falls back on simpler terminals. */
    draw_panel(s, right, "Styles");
    inner = cterm_rect_inset(right, 2);
    old_clip = cterm_set_clip(s, inner);
    x = cterm_text(s, inner.x, inner.y, cterm_style_make(0, 0, CTERM_BOLD), "bold ");
    x = cterm_text(s, x, inner.y, cterm_style_make(0, 0, CTERM_ITALIC), "italic ");
    x = cterm_text(s, x, inner.y, cterm_style_make(0, 0, CTERM_UNDERLINE), "underline ");
    x = cterm_text(s, x, inner.y, cterm_style_make(0, 0, CTERM_REVERSE), "reverse");
    (void)x;
    cterm_text(s, inner.x, inner.y + 1, cterm_style_make(0, 0, CTERM_DIM), "dim ");
    cterm_text(s, inner.x + 4, inner.y + 1, cterm_style_make(0, 0, CTERM_STRIKE), "strikethrough");

    cterm_text(s, inner.x, inner.y + 3, cterm_style_make(0, 0, CTERM_UNDERLINE), "True colour ramp");
    for (x = 0; x < inner.w; x++) {
        int value = inner.w > 1 ? x * 255 / (inner.w - 1) : 0;
        cterm_put(s, inner.x + x, inner.y + 4, ' ', cterm_style_make(CTERM_DEFAULT, CTERM_RGB(value, 80, 255 - value), 0));
    }
    cterm_set_clip(s, old_clip);

    cterm_present();
}

int main(void) {
    if (cterm_init() != 0) {
        fprintf(stderr, "cterm_init failed: %s\n", cterm_error());
        return 1;
    }
    for (;;) {
        cterm_event ev;
        draw();
        if (cterm_poll(&ev, -1) < 0) break;
        if (ev.type == CTERM_EVENT_KEY) break;
    }
    cterm_shutdown();
    return 0;
}
