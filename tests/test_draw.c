/* Tests for boxes, lines, formatted text and colour fallback. */
#define TIC_IMPLEMENTATION
#include "tic.h"
#include "test.h"

static const char *line(const tic_buffer *b, int y) {
    static char text[2048];
    tic_buffer_line(b, y, text, (int)sizeof text);
    return text;
}

static void test_box_ascii(void) {
    tic_buffer *b = tic_buffer_new(6, 4);
    tic_text(b, 1, 1, tic_plain(), "keep");
    tic_box(b, tic_rect_make(0, 0, 6, 4), TIC_BORDER_ASCII, tic_plain());
    CHECK_STR(line(b, 0), "+----+");
    CHECK_STR(line(b, 1), "|keep|"); /* the inside is left alone */
    CHECK_STR(line(b, 2), "|    |");
    CHECK_STR(line(b, 3), "+----+");
    tic_buffer_free(b);
}

static void test_box_unicode_styles(void) {
    tic_buffer *b = tic_buffer_new(3, 3);
    tic_box(b, tic_bounds(b), TIC_BORDER_SINGLE, tic_plain());
    CHECK_STR(line(b, 0), "\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x90");
    CHECK_STR(line(b, 1), "\xE2\x94\x82 \xE2\x94\x82");
    CHECK_STR(line(b, 2), "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x98");

    tic_clear(b);
    tic_box(b, tic_bounds(b), TIC_BORDER_DOUBLE, tic_plain());
    CHECK_STR(line(b, 0), "\xE2\x95\x94\xE2\x95\x90\xE2\x95\x97");

    tic_clear(b);
    tic_box(b, tic_bounds(b), TIC_BORDER_ROUNDED, tic_plain());
    CHECK_STR(line(b, 0), "\xE2\x95\xAD\xE2\x94\x80\xE2\x95\xAE");

    tic_clear(b);
    tic_box(b, tic_bounds(b), TIC_BORDER_HEAVY, tic_plain());
    CHECK_STR(line(b, 0), "\xE2\x94\x8F\xE2\x94\x81\xE2\x94\x93");
    tic_buffer_free(b);
}

static void test_box_small_sizes(void) {
    tic_buffer *b = tic_buffer_new(5, 3);
    /* Size zero and negative draw nothing. */
    tic_box(b, tic_rect_make(0, 0, 0, 3), TIC_BORDER_ASCII, tic_plain());
    tic_box(b, tic_rect_make(0, 0, 3, 0), TIC_BORDER_ASCII, tic_plain());
    tic_box(b, tic_rect_make(0, 0, -2, -2), TIC_BORDER_ASCII, tic_plain());
    CHECK_STR(line(b, 0), "     ");

    /* Too thin for corners. */
    tic_box(b, tic_rect_make(0, 0, 3, 1), TIC_BORDER_ASCII, tic_plain());
    CHECK_STR(line(b, 0), "---  ");
    tic_clear(b);
    tic_box(b, tic_rect_make(0, 0, 1, 3), TIC_BORDER_ASCII, tic_plain());
    CHECK_STR(line(b, 0), "|    ");
    CHECK_STR(line(b, 2), "|    ");

    /* Exactly 2x2 is all corners. */
    tic_clear(b);
    tic_box(b, tic_rect_make(0, 0, 2, 2), TIC_BORDER_ASCII, tic_plain());
    CHECK_STR(line(b, 0), "++   ");
    CHECK_STR(line(b, 1), "++   ");

    /* Partly off the buffer: must be clipped, not crash. */
    tic_clear(b);
    tic_box(b, tic_rect_make(-1, -1, 3, 3), TIC_BORDER_ASCII, tic_plain());
    CHECK_STR(line(b, 0), " |   ");
    CHECK_STR(line(b, 1), "-+   ");
    tic_clear(b);
    tic_box(b, tic_rect_make(3, 1, 100, 100), TIC_BORDER_ASCII, tic_plain());
    CHECK_STR(line(b, 1), "   +-");
    CHECK_STR(line(b, 2), "   | ");
    tic_buffer_free(b);
}

static void test_lines(void) {
    tic_buffer *b = tic_buffer_new(5, 3);
    tic_hline(b, 1, 1, 3, TIC_BORDER_ASCII, tic_plain());
    CHECK_STR(line(b, 1), " --- ");
    tic_clear(b);
    tic_vline(b, 2, 0, 2, TIC_BORDER_ASCII, tic_plain());
    CHECK_STR(line(b, 0), "  |  ");
    CHECK_STR(line(b, 1), "  |  ");
    CHECK_STR(line(b, 2), "     ");
    tic_hline(b, 0, 2, -3, TIC_BORDER_ASCII, tic_plain());
    CHECK_STR(line(b, 2), "     ");
    tic_buffer_free(b);
}

static void test_clip_applies_to_boxes(void) {
    tic_buffer *b = tic_buffer_new(6, 3);
    tic_set_clip(b, tic_rect_make(0, 0, 3, 3));
    tic_box(b, tic_bounds(b), TIC_BORDER_ASCII, tic_plain());
    CHECK_STR(line(b, 0), "+--   ");
    CHECK_STR(line(b, 1), "|     ");
    tic_buffer_free(b);
}

static void test_textf(void) {
    tic_buffer *b = tic_buffer_new(30, 1);
    CHECK_INT(tic_textf(b, 0, 0, tic_plain(), "%d/%s", 42, "x"), 4);
    CHECK_STR(line(b, 0), "42/x                          ");
    tic_buffer_free(b);
}

static void test_textf_long(void) {
    /* Longer than the stack buffer inside tic_textf. */
    enum { LENGTH = 1000 };
    static char text[LENGTH + 1];
    tic_buffer *b = tic_buffer_new(LENGTH, 1);
    memset(text, 'z', LENGTH);
    CHECK_INT(tic_textf(b, 0, 0, tic_plain(), "%s", text), LENGTH);
    CHECK_INT(tic__cell_at(b, LENGTH - 1, 0)->ch, 'z');
    tic_buffer_free(b);
}

static void test_color_macros(void) {
    CHECK_INT(TIC_DEFAULT, 0);
    CHECK_INT(TIC_COLOR256(255), 0x010000FF);
    CHECK_INT(TIC_RGB(1, 2, 3), 0x02010203);
    CHECK_INT(TIC_RGB(255, 255, 255), 0x02FFFFFF);
    /* Out-of-range values wrap rather than corrupting the colour's kind. */
    CHECK_INT(TIC_COLOR256(256), 0x01000000);
    CHECK_INT(TIC_RED, TIC_COLOR256(1));
    CHECK_INT(TIC_BRIGHT_WHITE, TIC_COLOR256(15));
}

static void test_downgrade_keeps_what_fits(void) {
    tic_color rgb = TIC_RGB(10, 20, 30);
    CHECK_INT(tic__downgrade(rgb, TIC_COLORS_RGB), rgb);
    CHECK_INT(tic__downgrade(TIC_COLOR256(200), TIC_COLORS_256), TIC_COLOR256(200));
    CHECK_INT(tic__downgrade(TIC_RED, TIC_COLORS_16), TIC_RED);
    CHECK_INT(tic__downgrade(TIC_DEFAULT, TIC_COLORS_16), TIC_DEFAULT);
}

static void test_downgrade_none_removes_color(void) {
    CHECK_INT(tic__downgrade(TIC_RGB(1, 2, 3), TIC_COLORS_NONE), TIC_DEFAULT);
    CHECK_INT(tic__downgrade(TIC_RED, TIC_COLORS_NONE), TIC_DEFAULT);
}

static void test_rgb_to_256(void) {
    /* Exact cube colours map to themselves. */
    CHECK_INT(tic__downgrade(TIC_RGB(255, 0, 0), TIC_COLORS_256), TIC_COLOR256(196));
    CHECK_INT(tic__downgrade(TIC_RGB(0, 255, 0), TIC_COLORS_256), TIC_COLOR256(46));
    CHECK_INT(tic__downgrade(TIC_RGB(0, 0, 255), TIC_COLORS_256), TIC_COLOR256(21));
    CHECK_INT(tic__downgrade(TIC_RGB(0, 0, 0), TIC_COLORS_256), TIC_COLOR256(16));
    CHECK_INT(tic__downgrade(TIC_RGB(255, 255, 255), TIC_COLORS_256), TIC_COLOR256(231));

    /* Greys prefer the grey ramp when it's closer. */
    CHECK_INT(tic__downgrade(TIC_RGB(128, 128, 128), TIC_COLORS_256), TIC_COLOR256(244));
    CHECK_INT(tic__downgrade(TIC_RGB(238, 238, 238), TIC_COLORS_256), TIC_COLOR256(255));
    CHECK_INT(tic__downgrade(TIC_RGB(8, 8, 8), TIC_COLORS_256), TIC_COLOR256(232));
}

static void test_round_trip_through_palette(void) {
    /* Every cube and grey colour must convert back to itself. */
    int i;
    for (i = 16; i < 256; i++) {
        int r, g, b, back;
        tic__256_to_rgb(i, &r, &g, &b);
        back = tic__rgb_to_256(r, g, b);
        /* A few cube colours are the same RGB as a grey, so compare the RGB values. */
        {
            int r2, g2, b2;
            tic__256_to_rgb(back, &r2, &g2, &b2);
            CHECK_INT(r2, r);
            CHECK_INT(g2, g);
            CHECK_INT(b2, b);
        }
    }
}

static void test_to_16(void) {
    CHECK_INT(tic__downgrade(TIC_RGB(255, 0, 0), TIC_COLORS_16), TIC_BRIGHT_RED);
    CHECK_INT(tic__downgrade(TIC_RGB(200, 10, 10), TIC_COLORS_16), TIC_RED);
    CHECK_INT(tic__downgrade(TIC_RGB(0, 0, 0), TIC_COLORS_16), TIC_BLACK);
    CHECK_INT(tic__downgrade(TIC_RGB(255, 255, 255), TIC_COLORS_16), TIC_BRIGHT_WHITE);
    CHECK_INT(tic__downgrade(TIC_COLOR256(196), TIC_COLORS_16), TIC_BRIGHT_RED);
    CHECK_INT(tic__downgrade(TIC_COLOR256(232), TIC_COLORS_16), TIC_BLACK);

    /* The result is always one of the 16, whatever goes in. */
    {
        int i, bad = 0;
        for (i = 0; i < 256; i++) {
            tic_color c = tic__downgrade(TIC_COLOR256(i), TIC_COLORS_16);
            if ((c & 0xFFu) >= 16) bad++;
        }
        CHECK_INT(bad, 0);
    }
}

static void test_color_detection(void) {
    CHECK_INT(tic__color_mode_from_env(NULL, NULL, NULL), TIC_COLORS_16);
    CHECK_INT(tic__color_mode_from_env(NULL, NULL, "xterm"), TIC_COLORS_16);
    CHECK_INT(tic__color_mode_from_env(NULL, NULL, "xterm-256color"), TIC_COLORS_256);
    CHECK_INT(tic__color_mode_from_env(NULL, NULL, "screen.xterm-256color"), TIC_COLORS_256);
    CHECK_INT(tic__color_mode_from_env(NULL, NULL, "xterm-direct"), TIC_COLORS_RGB);
    CHECK_INT(tic__color_mode_from_env(NULL, "truecolor", "xterm"), TIC_COLORS_RGB);
    CHECK_INT(tic__color_mode_from_env(NULL, "24bit", "xterm-256color"), TIC_COLORS_RGB);
    CHECK_INT(tic__color_mode_from_env(NULL, "yes", "xterm-256color"), TIC_COLORS_256);
    CHECK_INT(tic__color_mode_from_env(NULL, NULL, "dumb"), TIC_COLORS_NONE);
    CHECK_INT(tic__color_mode_from_env(NULL, "truecolor", "dumb"), TIC_COLORS_NONE);
    /* NO_COLOR wins over everything, but an empty value doesn't count. */
    CHECK_INT(tic__color_mode_from_env("1", "truecolor", "xterm-256color"), TIC_COLORS_NONE);
    CHECK_INT(tic__color_mode_from_env("", "truecolor", "xterm"), TIC_COLORS_RGB);
}

/* Returns the SGR sequence a style produces. */
static const char *sgr(tic_style style, tic_color_mode mode) {
    static char text[128];
    tic__sb sb;
    memset(&sb, 0, sizeof sb);
    tic__sgr_style(&sb, style, mode);
    tic__sb_append(&sb, "", 1);
    snprintf(text, sizeof text, "%s", sb.data + 1); /* skip the ESC to keep tests readable */
    tic__sb_free(&sb);
    return text;
}

static void test_sgr_output(void) {
    CHECK_STR(sgr(tic_plain(), TIC_COLORS_RGB), "[0m");
    CHECK_STR(sgr(tic_style_make(TIC_RED, TIC_BLUE, 0), TIC_COLORS_RGB), "[0;31;44m");
    CHECK_STR(sgr(tic_style_make(TIC_BRIGHT_RED, TIC_BRIGHT_BLUE, 0), TIC_COLORS_RGB), "[0;91;104m");
    CHECK_STR(sgr(tic_style_make(TIC_COLOR256(200), TIC_COLOR256(16), 0), TIC_COLORS_RGB), "[0;38;5;200;48;5;16m");
    CHECK_STR(sgr(tic_style_make(TIC_RGB(1, 2, 3), TIC_RGB(4, 5, 6), 0), TIC_COLORS_RGB), "[0;38;2;1;2;3;48;2;4;5;6m");
    CHECK_STR(sgr(tic_style_make(0, 0, TIC_BOLD | TIC_DIM | TIC_ITALIC | TIC_UNDERLINE | TIC_BLINK | TIC_REVERSE | TIC_STRIKE), TIC_COLORS_RGB),
              "[0;1;2;3;4;5;7;9m");

    /* Fallback is applied on the way out. */
    CHECK_STR(sgr(tic_style_make(TIC_RGB(255, 0, 0), TIC_DEFAULT, 0), TIC_COLORS_256), "[0;38;5;196m");
    CHECK_STR(sgr(tic_style_make(TIC_RGB(255, 0, 0), TIC_DEFAULT, 0), TIC_COLORS_16), "[0;91m");
    CHECK_STR(sgr(tic_style_make(TIC_RED, TIC_BLUE, TIC_BOLD), TIC_COLORS_NONE), "[0;1m");
}

int main(void) {
    RUN(test_box_ascii);
    RUN(test_box_unicode_styles);
    RUN(test_box_small_sizes);
    RUN(test_lines);
    RUN(test_clip_applies_to_boxes);
    RUN(test_textf);
    RUN(test_textf_long);
    RUN(test_color_macros);
    RUN(test_downgrade_keeps_what_fits);
    RUN(test_downgrade_none_removes_color);
    RUN(test_rgb_to_256);
    RUN(test_round_trip_through_palette);
    RUN(test_to_16);
    RUN(test_color_detection);
    RUN(test_sgr_output);
    return test_summary();
}
