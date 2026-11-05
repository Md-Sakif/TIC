/* Tests for cell buffers and basic drawing. No terminal needed. */
#define TIC_IMPLEMENTATION
#include "tic.h"
#include "test.h"

static const char *line(const tic_buffer *b, int y) {
    static char text[1024];
    tic_buffer_line(b, y, text, (int)sizeof text);
    return text;
}

static void test_new_and_resize(void) {
    tic_buffer *b = tic_buffer_new(4, 2);
    CHECK(b != NULL);
    CHECK_INT(b->width, 4);
    CHECK_INT(b->height, 2);
    CHECK_STR(line(b, 0), "    ");

    tic_text(b, 0, 0, tic_plain(), "abcd");
    CHECK_INT(tic_buffer_resize(b, 2, 1), 0);
    CHECK_INT(b->width, 2);
    CHECK_STR(line(b, 0), "  "); /* resizing blanks the contents */
    CHECK_INT(tic_buffer_line(b, 5, NULL, 0), -1);

    /* Zero and negative sizes are valid, and drawing on them does nothing. */
    CHECK_INT(tic_buffer_resize(b, 0, 0), 0);
    tic_text(b, 0, 0, tic_plain(), "x");
    tic_clear(b);
    CHECK_INT(tic_buffer_resize(b, -5, -5), 0);
    CHECK_INT(b->width, 0);
    tic_buffer_free(b);
    tic_buffer_free(NULL); /* must be safe */
}

static void test_text_and_return_value(void) {
    tic_buffer *b = tic_buffer_new(10, 2);
    int x = tic_text(b, 1, 0, tic_plain(), "ab");
    CHECK_INT(x, 3);
    x = tic_text(b, x, 0, tic_plain(), "cd");
    CHECK_INT(x, 5);
    CHECK_STR(line(b, 0), " abcd     ");
    CHECK_STR(line(b, 1), "          ");
    tic_buffer_free(b);
}

static void test_text_past_edges(void) {
    tic_buffer *b = tic_buffer_new(4, 1);
    /* Text is clipped, but the returned x still reflects its full length. */
    CHECK_INT(tic_text(b, 2, 0, tic_plain(), "abcd"), 6);
    CHECK_STR(line(b, 0), "  ab");
    tic_clear(b);
    CHECK_INT(tic_text(b, -2, 0, tic_plain(), "abcd"), 2);
    CHECK_STR(line(b, 0), "cd  ");
    tic_clear(b);
    tic_text(b, 0, -1, tic_plain(), "abcd");
    tic_text(b, 0, 1, tic_plain(), "abcd");
    CHECK_STR(line(b, 0), "    ");
    tic_buffer_free(b);
}

static void test_control_characters_are_replaced(void) {
    /* An attacker-controlled string must not reach the terminal as a command. */
    tic_buffer *b = tic_buffer_new(6, 1);
    tic_text(b, 0, 0, tic_plain(), "\x1b[2J");
    CHECK_INT(tic__cell_at(b, 0, 0)->ch, TIC__REPLACEMENT);
    CHECK_INT(tic__cell_at(b, 1, 0)->ch, '[');
    tic_clear(b);
    tic_put(b, 0, 0, 0x9B, tic_plain()); /* the one-byte form of ESC [ */
    CHECK_INT(tic__cell_at(b, 0, 0)->ch, TIC__REPLACEMENT);
    tic_put(b, 0, 0, 0x7F, tic_plain());
    CHECK_INT(tic__cell_at(b, 0, 0)->ch, TIC__REPLACEMENT);
    tic_put(b, 0, 0, 0x110000, tic_plain());
    CHECK_INT(tic__cell_at(b, 0, 0)->ch, TIC__REPLACEMENT);
    tic_put(b, 0, 0, 0xD800, tic_plain());
    CHECK_INT(tic__cell_at(b, 0, 0)->ch, TIC__REPLACEMENT);
    tic_buffer_free(b);
}

static void test_wide_characters(void) {
    tic_buffer *b = tic_buffer_new(6, 1);
    const char *zhong = "\xE4\xB8\xAD"; /* a CJK character, two cells wide */

    CHECK_INT(tic_text(b, 1, 0, tic_plain(), zhong), 3);
    CHECK_INT(tic__cell_at(b, 1, 0)->ch, 0x4E2D);
    CHECK_INT(tic__cell_at(b, 2, 0)->ch, 0); /* continuation */

    /* Overwriting the left half removes the right half. */
    tic_put(b, 1, 0, 'x', tic_plain());
    CHECK_STR(line(b, 0), " x    ");

    /* Overwriting the right half removes the left half. */
    tic_clear(b);
    tic_text(b, 1, 0, tic_plain(), zhong);
    tic_put(b, 2, 0, 'x', tic_plain());
    CHECK_STR(line(b, 0), "  x   ");

    /* A wide character overlapping another one by a single cell. */
    tic_clear(b);
    tic_text(b, 2, 0, tic_plain(), zhong);
    tic_text(b, 1, 0, tic_plain(), zhong);
    CHECK_INT(tic__cell_at(b, 1, 0)->ch, 0x4E2D);
    CHECK_INT(tic__cell_at(b, 2, 0)->ch, 0);
    CHECK_INT(tic__cell_at(b, 3, 0)->ch, ' ');

    /* And from the other side. */
    tic_clear(b);
    tic_text(b, 1, 0, tic_plain(), zhong);
    tic_text(b, 2, 0, tic_plain(), zhong);
    CHECK_INT(tic__cell_at(b, 1, 0)->ch, ' ');
    CHECK_INT(tic__cell_at(b, 2, 0)->ch, 0x4E2D);
    CHECK_INT(tic__cell_at(b, 3, 0)->ch, 0);

    /* No room for the right half: draw a space rather than half a character. */
    tic_clear(b);
    tic_text(b, 5, 0, tic_plain(), zhong);
    CHECK_INT(tic__cell_at(b, 5, 0)->ch, ' ');

    /* Left half off-screen: same idea. */
    tic_clear(b);
    tic_text(b, -1, 0, tic_plain(), zhong);
    CHECK_INT(tic__cell_at(b, 0, 0)->ch, ' ');
    tic_buffer_free(b);
}

static void test_combining_marks(void) {
    tic_buffer *b = tic_buffer_new(5, 1);
    const tic_cell *cell;

    /* Bengali KU: the consonant KA plus a vowel sign that sits on it. */
    CHECK_INT(tic_text(b, 0, 0, tic_plain(), "\xE0\xA6\x95\xE0\xA7\x81x"), 2);
    cell = tic__cell_at(b, 0, 0);
    CHECK_INT(cell->ch, 0x995);
    CHECK_INT(cell->marks[0], 0x9C1);
    CHECK_INT(tic__cell_at(b, 1, 0)->ch, 'x');

    /* A mark at the start of a string has nothing to attach to. */
    tic_clear(b);
    tic_text(b, 1, 0, tic_plain(), "a");
    CHECK_INT(tic_text(b, 2, 0, tic_plain(), "\xCC\x81"), 2);
    CHECK_INT(tic__cell_at(b, 1, 0)->marks[0], 0);

    /* tic_put with a mark attaches to the cell on its left. */
    tic_put(b, 2, 0, 0x301, tic_plain());
    CHECK_INT(tic__cell_at(b, 1, 0)->marks[0], 0x301);

    /* Marks attach to the left half of a wide character. */
    tic_clear(b);
    tic_text(b, 0, 0, tic_plain(), "\xE4\xB8\xAD\xCC\x81");
    CHECK_INT(tic__cell_at(b, 0, 0)->marks[0], 0x301);

    /* Marks past the limit are dropped safely. */
    tic_clear(b);
    tic_text(b, 0, 0, tic_plain(), "e\xCC\x81\xCC\x82\xCC\x83\xCC\x84\xCC\x85\xCC\x86");
    CHECK_INT(tic__cell_at(b, 0, 0)->marks[TIC_MAX_MARKS - 1], 0x301 + TIC_MAX_MARKS - 1);

    /* Overwriting a cell removes its marks. */
    tic_put(b, 0, 0, 'z', tic_plain());
    CHECK_INT(tic__cell_at(b, 0, 0)->marks[0], 0);
    tic_buffer_free(b);
}

static void test_clip(void) {
    tic_buffer *b = tic_buffer_new(6, 3);
    tic_rect old = tic_set_clip(b, tic_rect_make(1, 1, 3, 1));
    CHECK_INT(old.w, 6);

    tic_text(b, 0, 1, tic_plain(), "abcdef");
    tic_text(b, 0, 0, tic_plain(), "zzz");
    tic_text(b, 0, 2, tic_plain(), "zzz");
    CHECK_STR(line(b, 0), "      ");
    CHECK_STR(line(b, 1), " bcd  ");
    CHECK_STR(line(b, 2), "      ");

    /* A wide character crossing the clip edge turns into a space. */
    tic_text(b, 3, 1, tic_plain(), "\xE4\xB8\xAD");
    CHECK_INT(tic__cell_at(b, 3, 1)->ch, ' ');
    CHECK_INT(tic__cell_at(b, 4, 1)->ch, ' ');

    /* A clip area can't extend past the buffer. */
    tic_set_clip(b, tic_rect_make(-5, -5, 100, 100));
    CHECK_INT(b->clip.x, 0);
    CHECK_INT(b->clip.w, 6);

    /* Clearing removes the clip. */
    tic_set_clip(b, tic_rect_make(0, 0, 1, 1));
    tic_clear(b);
    CHECK_INT(b->clip.w, 6);
    tic_buffer_free(b);
}

static void test_fill(void) {
    tic_buffer *b = tic_buffer_new(5, 3);
    tic_fill(b, tic_rect_make(1, 1, 3, 2), '#', tic_plain());
    CHECK_STR(line(b, 0), "     ");
    CHECK_STR(line(b, 1), " ### ");
    CHECK_STR(line(b, 2), " ### ");

    /* A rectangle partly off the buffer is handled. */
    tic_clear(b);
    tic_fill(b, tic_rect_make(-2, -2, 4, 3), '#', tic_plain());
    CHECK_STR(line(b, 0), "##   ");
    CHECK_STR(line(b, 1), "     ");

    /* Wide characters fill in steps of two. */
    tic_clear(b);
    tic_fill(b, tic_rect_make(0, 0, 5, 1), 0x4E2D, tic_plain());
    CHECK_INT(tic__cell_at(b, 0, 0)->ch, 0x4E2D);
    CHECK_INT(tic__cell_at(b, 2, 0)->ch, 0x4E2D);
    CHECK_INT(tic__cell_at(b, 4, 0)->ch, ' ');

    /* A mark has no width, so it fills nothing, and must not hang. */
    tic_clear(b);
    tic_fill(b, tic_rect_make(0, 0, 5, 1), 0x301, tic_plain());
    CHECK_STR(line(b, 0), "     ");
    tic_buffer_free(b);
}

static void test_rect_helpers(void) {
    tic_rect r = tic_rect_intersect(tic_rect_make(0, 0, 5, 5), tic_rect_make(3, 3, 5, 5));
    CHECK_INT(r.x, 3);
    CHECK_INT(r.w, 2);
    r = tic_rect_intersect(tic_rect_make(0, 0, 2, 2), tic_rect_make(5, 5, 2, 2));
    CHECK_INT(r.w, 0);
    CHECK_INT(r.h, 0);
    CHECK(tic_rect_contains(tic_rect_make(1, 1, 2, 2), 1, 1));
    CHECK(tic_rect_contains(tic_rect_make(1, 1, 2, 2), 2, 2));
    CHECK(!tic_rect_contains(tic_rect_make(1, 1, 2, 2), 3, 1));
    CHECK(!tic_rect_contains(tic_rect_make(1, 1, 2, 2), 0, 1));
    r = tic_rect_inset(tic_rect_make(0, 0, 10, 4), 1);
    CHECK_INT(r.x, 1);
    CHECK_INT(r.w, 8);
    CHECK_INT(r.h, 2);
    r = tic_rect_inset(tic_rect_make(0, 0, 1, 1), 5);
    CHECK_INT(r.w, 0);
}

static void test_line_respects_capacity(void) {
    tic_buffer *b = tic_buffer_new(5, 1);
    char small[4];
    tic_text(b, 0, 0, tic_plain(), "abcde");
    CHECK_INT(tic_buffer_line(b, 0, small, (int)sizeof small), 3);
    CHECK_STR(small, "abc");
    tic_buffer_free(b);
}

int main(void) {
    RUN(test_new_and_resize);
    RUN(test_text_and_return_value);
    RUN(test_text_past_edges);
    RUN(test_control_characters_are_replaced);
    RUN(test_wide_characters);
    RUN(test_combining_marks);
    RUN(test_clip);
    RUN(test_fill);
    RUN(test_rect_helpers);
    RUN(test_line_respects_capacity);
    return test_summary();
}
