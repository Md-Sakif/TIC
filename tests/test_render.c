/* Tests for the diff renderer. No terminal needed: output goes to tests/vt.h. */
#define TIC_IMPLEMENTATION
#include "tic.h"
#include "test.h"
#include "vt.h"

/* Everything a test needs to stand in for a terminal. */
typedef struct fixture {
    tic_buffer *front, *back;
    tic__render_state state;
    tic__sb out;
    vt term;
} fixture;

static void fixture_init(fixture *f, int width, int height) {
    memset(f, 0, sizeof *f);
    f->front = tic_buffer_new(width, height);
    f->back = tic_buffer_new(width, height);
    tic__render_state_reset(&f->state);
    vt_init(&f->term, width, height);
}

static void fixture_free(fixture *f) {
    tic_buffer_free(f->front);
    tic_buffer_free(f->back);
    tic__sb_free(&f->out);
    vt_free(&f->term);
}

/* Renders one frame and returns how many bytes it took. */
static size_t present(fixture *f, int full) {
    f->out.len = 0;
    tic__render(&f->out, f->front, f->back, &f->state, TIC_COLORS_RGB, full);
    vt_feed(&f->term, f->out.data, f->out.len);
    return f->out.len;
}

/* A small repeatable random generator. rand() differs between systems, and a
 * failure here needs to reproduce the same way on every machine. */
static uint32_t rng_state;
static uint32_t rng(void) {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state >> 8;
}

static void test_first_frame_paints_everything(void) {
    fixture f;
    fixture_init(&f, 10, 2);
    tic_text(f.back, 0, 0, tic_plain(), "hello");
    present(&f, 1);
    CHECK_INT(vt_compare(&f.term, f.back), 0);
    fixture_free(&f);
}

static void test_no_change_sends_nothing(void) {
    fixture f;
    fixture_init(&f, 10, 2);
    tic_text(f.back, 0, 0, tic_plain(), "hello");
    present(&f, 1);
    CHECK_INT(present(&f, 0), 0);

    /* Redrawing identical content, as an immediate-mode app does every frame. */
    tic_clear(f.back);
    tic_text(f.back, 0, 0, tic_plain(), "hello");
    CHECK_INT(present(&f, 0), 0);
    fixture_free(&f);
}

static void test_only_changed_cells_are_sent(void) {
    fixture f;
    size_t bytes;
    fixture_init(&f, 200, 50);
    tic_fill(f.back, tic_bounds(f.back), 'x', tic_plain());
    bytes = present(&f, 1);
    CHECK(bytes >= 10000); /* the first frame sends the whole screen */

    /* One changed cell in a 10,000-cell screen: sync, move, style, char, sync. */
    tic_put(f.back, 100, 25, 'y', tic_plain());
    bytes = present(&f, 0);
    CHECK(bytes > 0);
    CHECK(bytes < 40);
    CHECK_INT(vt_compare(&f.term, f.back), 0);
    fixture_free(&f);
}

static void test_adjacent_cells_share_a_move_and_style(void) {
    fixture f;
    size_t one, many;
    tic_style red = tic_style_make(TIC_RED, TIC_DEFAULT, 0);
    fixture_init(&f, 40, 2);
    present(&f, 1);

    tic_put(f.back, 5, 0, 'a', red);
    one = present(&f, 0);

    /* Twelve more cells should only cost twelve more bytes. */
    tic_text(f.back, 10, 1, red, "abcdefghijkl");
    many = present(&f, 0);
    CHECK(many <= one + 12);
    CHECK_INT(vt_compare(&f.term, f.back), 0);
    fixture_free(&f);
}

static void test_styles(void) {
    fixture f;
    fixture_init(&f, 30, 3);
    tic_text(f.back, 0, 0, tic_style_make(TIC_RED, TIC_BLUE, TIC_BOLD), "a");
    tic_text(f.back, 1, 0, tic_style_make(TIC_COLOR256(200), TIC_DEFAULT, TIC_UNDERLINE), "b");
    tic_text(f.back, 2, 0, tic_style_make(TIC_RGB(1, 2, 3), TIC_RGB(4, 5, 6), TIC_ITALIC), "c");
    tic_text(f.back, 3, 0, tic_plain(), "d"); /* must fully reset the previous style */
    tic_text(f.back, 4, 0, tic_style_make(0, 0, TIC_DIM | TIC_BLINK | TIC_REVERSE | TIC_STRIKE), "e");
    present(&f, 1);
    CHECK_INT(vt_compare(&f.term, f.back), 0);
    fixture_free(&f);
}

static void test_style_only_change(void) {
    fixture f;
    fixture_init(&f, 10, 1);
    tic_text(f.back, 0, 0, tic_plain(), "abc");
    present(&f, 1);
    tic_text(f.back, 0, 0, tic_style_make(TIC_GREEN, TIC_DEFAULT, 0), "abc");
    CHECK(present(&f, 0) > 0);
    CHECK_INT(vt_compare(&f.term, f.back), 0);
    fixture_free(&f);
}

static void test_wide_and_marks(void) {
    fixture f;
    fixture_init(&f, 12, 2);
    tic_text(f.back, 0, 0, tic_plain(), "\xE4\xB8\xAD\xE6\x96\x87x");
    tic_text(f.back, 0, 1, tic_plain(), "\xE0\xA6\x95\xE0\xA7\x81 \xF0\x9F\x98\x80z");
    present(&f, 1);
    CHECK_INT(vt_compare(&f.term, f.back), 0);

    /* Replacing a wide character with two narrow ones, and the reverse. */
    tic_text(f.back, 0, 0, tic_plain(), "ab");
    present(&f, 0);
    CHECK_INT(vt_compare(&f.term, f.back), 0);
    tic_text(f.back, 0, 0, tic_plain(), "\xE4\xB8\xAD");
    present(&f, 0);
    CHECK_INT(vt_compare(&f.term, f.back), 0);

    /* Shifting a wide character over by one, which overlaps its old position. */
    tic_clear(f.back);
    tic_text(f.back, 1, 0, tic_plain(), "\xE4\xB8\xAD");
    present(&f, 0);
    CHECK_INT(vt_compare(&f.term, f.back), 0);
    fixture_free(&f);
}

static void test_disputed_widths_force_a_move(void) {
    /* Terminals disagree about these widths, so the next cell has to be positioned
     * explicitly: the output must hold one cursor move for every character. */
    fixture f;
    size_t i;
    int moves = 0;
    fixture_init(&f, 10, 1);
    present(&f, 1);
    tic_text(f.back, 0, 0, tic_plain(), "\xE0\xA6\x95\xE0\xA6\xBE" "x"); /* Bengali KA, AA, then x */
    present(&f, 0);
    for (i = 0; i + 1 < f.out.len; i++) {
        size_t j = i + 2;
        if (f.out.data[i] != 0x1b || f.out.data[i + 1] != '[') continue;
        while (j < f.out.len && ((f.out.data[j] >= '0' && f.out.data[j] <= '9') || f.out.data[j] == ';')) j++;
        if (j < f.out.len && f.out.data[j] == 'H') moves++;
    }
    CHECK_INT(moves, 3);
    CHECK_INT(vt_compare(&f.term, f.back), 0);
    CHECK_INT(tic__width_is_disputed(0x995), 1);
    CHECK_INT(tic__width_is_disputed(0x1F600), 1);
    CHECK_INT(tic__width_is_disputed('a'), 0);
    CHECK_INT(tic__width_is_disputed(0x2500), 0); /* box drawing stays cheap */
    fixture_free(&f);
}

static void test_bottom_right_corner(void) {
    /* The classic trap: this cell must not make the screen scroll. */
    fixture f;
    fixture_init(&f, 4, 2);
    tic_put(f.back, 3, 1, 'z', tic_plain());
    tic_put(f.back, 3, 0, 'y', tic_plain());
    present(&f, 1);
    CHECK_INT(vt_compare(&f.term, f.back), 0);
    fixture_free(&f);
}

static void test_cursor(void) {
    fixture f;
    fixture_init(&f, 10, 2);
    present(&f, 1);
    CHECK_INT(f.term.cursor_visible, 0);

    tic_show_cursor(f.back, 3, 1);
    present(&f, 0);
    CHECK_INT(f.term.cursor_visible, 1);
    CHECK_INT(f.term.x, 3);
    CHECK_INT(f.term.y, 1);

    /* Repeating the same state costs nothing. */
    CHECK_INT(present(&f, 0), 0);

    /* A cell change hides the cursor while painting, then brings it back. */
    tic_put(f.back, 0, 0, 'a', tic_plain());
    present(&f, 0);
    CHECK_INT(f.term.cursor_visible, 1);
    CHECK_INT(f.term.x, 3);
    CHECK_INT(f.term.y, 1);

    /* Moving it costs a move and nothing else. */
    tic_show_cursor(f.back, 5, 0);
    present(&f, 0);
    CHECK_INT(f.term.x, 5);
    CHECK_INT(f.term.y, 0);

    tic_hide_cursor(f.back);
    present(&f, 0);
    CHECK_INT(f.term.cursor_visible, 0);

    /* A cursor outside the clip area stays hidden. */
    tic_set_clip(f.back, tic_rect_make(0, 0, 2, 2));
    tic_show_cursor(f.back, 5, 0);
    CHECK_INT(f.back->cursor_visible, 0);
    fixture_free(&f);
}

static void test_full_repaint_recovers(void) {
    fixture f;
    fixture_init(&f, 10, 2);
    tic_text(f.back, 0, 0, tic_style_make(TIC_RED, TIC_DEFAULT, 0), "hello");
    present(&f, 1);

    /* Something else scribbles over the terminal... */
    vt_feed(&f.term, "\x1b[1;1Hgarbage", 13);
    CHECK_INT(vt_at(&f.term, 0, 0)->ch, 'g');

    /* ...and a full repaint fixes it. */
    present(&f, 1);
    CHECK_INT(vt_compare(&f.term, f.back), 0);
    CHECK_INT(f.term.sync_depth_errors, 0);
    fixture_free(&f);
}

/* Draws something random: a mix of plain text, wide characters, combining marks,
 * emoji and styles, deliberately overlapping and running off the edges. */
static void random_drawing(tic_buffer *b) {
    static const char *const samples[] = {
        "plain", "x", "\xE4\xB8\xAD\xE6\x96\x87", "\xF0\x9F\x98\x80", "e\xCC\x81",
        "\xE0\xA6\x95\xE0\xA7\x81\xE0\xA6\x95\xE0\xA6\xBE", "a\xE4\xB8\xAD\x62", "\xC3\xA9",
        "\xE2\x94\x80\xE2\x94\x82", "\xE4\xB8\xAD\xCC\x81", " "
    };
    static const tic_color colors[] = {
        TIC_DEFAULT, TIC_RED, TIC_BRIGHT_CYAN, TIC_COLOR256(123), TIC_RGB(10, 20, 30)
    };
    int ops = (int)(rng() % 8), i;
    for (i = 0; i < ops; i++) {
        tic_style style = tic_style_make(colors[rng() % 5], colors[rng() % 5], rng() % 128);
        int x = (int)(rng() % (uint32_t)(b->width + 4)) - 2;
        int y = (int)(rng() % (uint32_t)(b->height + 2)) - 1;
        switch (rng() % 6) {
            case 0:
                tic_fill(b, tic_rect_make(x, y, (int)(rng() % 6), (int)(rng() % 3)), rng() % 2 ? '#' : 0x4E2D, style);
                break;
            case 1:
                tic_set_clip(b, tic_rect_make(x, y, (int)(rng() % 8) + 1, (int)(rng() % 4) + 1));
                break;
            case 2:
                tic_set_clip(b, tic_bounds(b));
                break;
            default:
                tic_text(b, x, y, style, samples[rng() % (sizeof samples / sizeof samples[0])]);
                break;
        }
    }
    tic_set_clip(b, tic_bounds(b));
    if (rng() % 4 == 0) tic_show_cursor(b, (int)(rng() % (uint32_t)b->width), (int)(rng() % (uint32_t)b->height));
    else if (rng() % 4 == 0) tic_hide_cursor(b);
}

/* The most important renderer test. Whatever gets drawn, in any order, the screen
 * has to end up exactly like the back buffer after every single frame. */
static void test_random_frames_always_match(void) {
    int run;
    rng_state = 12345;
    for (run = 0; run < 40; run++) {
        fixture f;
        int width = (int)(rng() % 30) + 1, height = (int)(rng() % 8) + 1, frame;
        fixture_init(&f, width, height);
        present(&f, 1);
        for (frame = 0; frame < 150; frame++) {
            int differences;
            if (rng() % 10 == 0) tic_clear(f.back);
            random_drawing(f.back);
            present(&f, rng() % 40 == 0);
            differences = vt_compare(&f.term, f.back);
            CHECK_INT(differences, 0);
            if (differences) break;
            /* Cursor state has to match as well, and updates have to be well formed. */
            CHECK_INT(f.term.cursor_visible, f.back->cursor_visible);
            if (f.back->cursor_visible) {
                CHECK_INT(f.term.x, f.back->cursor_x);
                CHECK_INT(f.term.y, f.back->cursor_y);
            }
            CHECK_INT(f.term.in_sync, 0);
            CHECK_INT(f.term.sync_depth_errors, 0);
        }
        fixture_free(&f);
    }
}

int main(void) {
    RUN(test_first_frame_paints_everything);
    RUN(test_no_change_sends_nothing);
    RUN(test_only_changed_cells_are_sent);
    RUN(test_adjacent_cells_share_a_move_and_style);
    RUN(test_styles);
    RUN(test_style_only_change);
    RUN(test_wide_and_marks);
    RUN(test_disputed_widths_force_a_move);
    RUN(test_bottom_right_corner);
    RUN(test_cursor);
    RUN(test_full_repaint_recovers);
    RUN(test_random_frames_always_match);
    return test_summary();
}
