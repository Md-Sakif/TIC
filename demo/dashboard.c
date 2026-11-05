/* dashboard.c - a static screen showing off the drawing API: boxes, colours, text in
 * several scripts, and clipping. It redraws when the terminal is resized. Press any
 * key to quit. */
#define TIC_IMPLEMENTATION
#include "tic.h"

#include <stdio.h>

static void draw_bar(tic_buffer *s, int x, int y, int width, int percent, tic_color color) {
    int filled = width * percent / 100;
    tic_fill(s, tic_rect_make(x, y, filled, 1), 0x2588, tic_style_make(color, TIC_DEFAULT, 0)); /* full block */
    tic_fill(s, tic_rect_make(x + filled, y, width - filled, 1), 0x2591,
             tic_style_make(TIC_BRIGHT_BLACK, TIC_DEFAULT, 0)); /* light shade */
    tic_textf(s, x + width + 1, y, tic_plain(), "%3d%%", percent);
}

static void draw_panel(tic_buffer *s, tic_rect area, const char *title) {
    tic_box(s, area, TIC_BORDER_ROUNDED, tic_style_make(TIC_BRIGHT_BLUE, TIC_DEFAULT, 0));
    tic_textf(s, area.x + 2, area.y, tic_style_make(TIC_BRIGHT_WHITE, TIC_DEFAULT, TIC_BOLD), " %s ", title);
}

static void draw(void) {
    tic_buffer *s = tic_screen();
    int half = s->width / 2;
    tic_rect left = tic_rect_make(0, 1, half, s->height - 2);
    tic_rect right = tic_rect_make(half, 1, s->width - half, s->height - 2);
    tic_rect inner;
    tic_rect old_clip;
    int x, bar_width;

    static const char *const mode_names[] = { "no colour", "16 colours", "256 colours", "true colour" };

    tic_clear(s);

    /* Header and footer, each a full-width bar. */
    tic_fill(s, tic_rect_make(0, 0, s->width, 1), ' ', tic_style_make(TIC_BLACK, TIC_CYAN, 0));
    tic_text(s, 1, 0, tic_style_make(TIC_BLACK, TIC_CYAN, TIC_BOLD), "TIC dashboard");
    tic_textf(s, 1, s->height - 1, tic_style_make(TIC_BRIGHT_BLACK, TIC_DEFAULT, 0),
              "%dx%d, %s. Press any key to quit.", s->width, s->height, mode_names[tic_get_color_mode()]);

    /* Left: progress bars, and text in several scripts. */
    draw_panel(s, left, "Status");
    inner = tic_rect_inset(left, 2);
    bar_width = inner.w - 6;
    old_clip = tic_set_clip(s, inner);
    draw_bar(s, inner.x, inner.y, bar_width, 72, TIC_GREEN);
    draw_bar(s, inner.x, inner.y + 1, bar_width, 35, TIC_YELLOW);
    draw_bar(s, inner.x, inner.y + 2, bar_width, 91, TIC_RED);

    tic_text(s, inner.x, inner.y + 4, tic_style_make(TIC_DEFAULT, TIC_DEFAULT, TIC_UNDERLINE), "Text");
    tic_text(s, inner.x, inner.y + 5, tic_plain(), "English: Hello");
    tic_text(s, inner.x, inner.y + 6, tic_plain(), "\xE0\xA6\xAC\xE0\xA6\xBE\xE0\xA6\x82\xE0\xA6\xB2\xE0\xA6\xBE: \xE0\xA6\xB9\xE0\xA7\x8D\xE0\xA6\xAF\xE0\xA6\xBE\xE0\xA6\xB2\xE0\xA7\x8B");
    tic_text(s, inner.x, inner.y + 7, tic_plain(), "\xE4\xB8\xAD\xE6\x96\x87: \xE4\xBD\xA0\xE5\xA5\xBD");
    tic_text(s, inner.x, inner.y + 8, tic_plain(), "Emoji: \xF0\x9F\x8E\x89 \xF0\x9F\x9A\x80");
    tic_set_clip(s, old_clip);

    /* Right: styles and a colour ramp that falls back on simpler terminals. */
    draw_panel(s, right, "Styles");
    inner = tic_rect_inset(right, 2);
    old_clip = tic_set_clip(s, inner);
    x = tic_text(s, inner.x, inner.y, tic_style_make(0, 0, TIC_BOLD), "bold ");
    x = tic_text(s, x, inner.y, tic_style_make(0, 0, TIC_ITALIC), "italic ");
    x = tic_text(s, x, inner.y, tic_style_make(0, 0, TIC_UNDERLINE), "underline ");
    x = tic_text(s, x, inner.y, tic_style_make(0, 0, TIC_REVERSE), "reverse");
    (void)x;
    tic_text(s, inner.x, inner.y + 1, tic_style_make(0, 0, TIC_DIM), "dim ");
    tic_text(s, inner.x + 4, inner.y + 1, tic_style_make(0, 0, TIC_STRIKE), "strikethrough");

    tic_text(s, inner.x, inner.y + 3, tic_style_make(0, 0, TIC_UNDERLINE), "True colour ramp");
    for (x = 0; x < inner.w; x++) {
        int value = inner.w > 1 ? x * 255 / (inner.w - 1) : 0;
        tic_put(s, inner.x + x, inner.y + 4, ' ', tic_style_make(TIC_DEFAULT, TIC_RGB(value, 80, 255 - value), 0));
    }
    tic_set_clip(s, old_clip);

    tic_present();
}

int main(void) {
    if (tic_init() != 0) {
        fprintf(stderr, "tic_init failed: %s\n", tic_error());
        return 1;
    }
    for (;;) {
        tic_event ev;
        draw();
        if (tic_poll(&ev, -1) < 0) break;
        if (ev.type == TIC_EVENT_KEY) break;
    }
    tic_shutdown();
    return 0;
}
