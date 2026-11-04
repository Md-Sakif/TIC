/* Tests for cell buffers and basic drawing. No terminal needed. */
#define CTERM_IMPLEMENTATION
#include "cterm.h"
#include "test.h"

static const char *line(const cterm_buffer *b, int y) {
    static char text[1024];
    cterm_buffer_line(b, y, text, (int)sizeof text);
    return text;
}

static void test_new_and_resize(void) {
    cterm_buffer *b = cterm_buffer_new(4, 2);
    CHECK(b != NULL);
    CHECK_INT(b->width, 4);
    CHECK_INT(b->height, 2);
    CHECK_STR(line(b, 0), "    ");

    cterm_text(b, 0, 0, cterm_plain(), "abcd");
    CHECK_INT(cterm_buffer_resize(b, 2, 1), 0);
    CHECK_INT(b->width, 2);
    CHECK_STR(line(b, 0), "  "); /* resizing blanks the contents */
    CHECK_INT(cterm_buffer_line(b, 5, NULL, 0), -1);

    /* Zero and negative sizes are valid, and drawing on them does nothing. */
    CHECK_INT(cterm_buffer_resize(b, 0, 0), 0);
    cterm_text(b, 0, 0, cterm_plain(), "x");
    cterm_clear(b);
    CHECK_INT(cterm_buffer_resize(b, -5, -5), 0);
    CHECK_INT(b->width, 0);
    cterm_buffer_free(b);
    cterm_buffer_free(NULL); /* must be safe */
}

static void test_text_and_return_value(void) {
    cterm_buffer *b = cterm_buffer_new(10, 2);
    int x = cterm_text(b, 1, 0, cterm_plain(), "ab");
    CHECK_INT(x, 3);
    x = cterm_text(b, x, 0, cterm_plain(), "cd");
    CHECK_INT(x, 5);
    CHECK_STR(line(b, 0), " abcd     ");
    CHECK_STR(line(b, 1), "          ");
    cterm_buffer_free(b);
}

static void test_text_past_edges(void) {
    cterm_buffer *b = cterm_buffer_new(4, 1);
    /* Text is clipped, but the returned x still reflects its full length. */
    CHECK_INT(cterm_text(b, 2, 0, cterm_plain(), "abcd"), 6);
    CHECK_STR(line(b, 0), "  ab");
    cterm_clear(b);
    CHECK_INT(cterm_text(b, -2, 0, cterm_plain(), "abcd"), 2);
    CHECK_STR(line(b, 0), "cd  ");
    cterm_clear(b);
    cterm_text(b, 0, -1, cterm_plain(), "abcd");
    cterm_text(b, 0, 1, cterm_plain(), "abcd");
    CHECK_STR(line(b, 0), "    ");
    cterm_buffer_free(b);
}

static void test_control_characters_are_replaced(void) {
    /* An attacker-controlled string must not reach the terminal as a command. */
    cterm_buffer *b = cterm_buffer_new(6, 1);
    cterm_text(b, 0, 0, cterm_plain(), "\x1b[2J");
    CHECK_INT(cterm__cell_at(b, 0, 0)->ch, CTERM__REPLACEMENT);
    CHECK_INT(cterm__cell_at(b, 1, 0)->ch, '[');
    cterm_clear(b);
    cterm_put(b, 0, 0, 0x9B, cterm_plain()); /* the one-byte form of ESC [ */
    CHECK_INT(cterm__cell_at(b, 0, 0)->ch, CTERM__REPLACEMENT);
    cterm_put(b, 0, 0, 0x7F, cterm_plain());
    CHECK_INT(cterm__cell_at(b, 0, 0)->ch, CTERM__REPLACEMENT);
    cterm_put(b, 0, 0, 0x110000, cterm_plain());
    CHECK_INT(cterm__cell_at(b, 0, 0)->ch, CTERM__REPLACEMENT);
    cterm_put(b, 0, 0, 0xD800, cterm_plain());
    CHECK_INT(cterm__cell_at(b, 0, 0)->ch, CTERM__REPLACEMENT);
    cterm_buffer_free(b);
}

static void test_wide_characters(void) {
    cterm_buffer *b = cterm_buffer_new(6, 1);
    const char *zhong = "\xE4\xB8\xAD"; /* a CJK character, two cells wide */

    CHECK_INT(cterm_text(b, 1, 0, cterm_plain(), zhong), 3);
    CHECK_INT(cterm__cell_at(b, 1, 0)->ch, 0x4E2D);
    CHECK_INT(cterm__cell_at(b, 2, 0)->ch, 0); /* continuation */

    /* Overwriting the left half removes the right half. */
    cterm_put(b, 1, 0, 'x', cterm_plain());
    CHECK_STR(line(b, 0), " x    ");

    /* Overwriting the right half removes the left half. */
    cterm_clear(b);
    cterm_text(b, 1, 0, cterm_plain(), zhong);
    cterm_put(b, 2, 0, 'x', cterm_plain());
    CHECK_STR(line(b, 0), "  x   ");

    /* A wide character overlapping another one by a single cell. */
    cterm_clear(b);
    cterm_text(b, 2, 0, cterm_plain(), zhong);
    cterm_text(b, 1, 0, cterm_plain(), zhong);
    CHECK_INT(cterm__cell_at(b, 1, 0)->ch, 0x4E2D);
    CHECK_INT(cterm__cell_at(b, 2, 0)->ch, 0);
    CHECK_INT(cterm__cell_at(b, 3, 0)->ch, ' ');

    /* And from the other side. */
    cterm_clear(b);
    cterm_text(b, 1, 0, cterm_plain(), zhong);
    cterm_text(b, 2, 0, cterm_plain(), zhong);
    CHECK_INT(cterm__cell_at(b, 1, 0)->ch, ' ');
    CHECK_INT(cterm__cell_at(b, 2, 0)->ch, 0x4E2D);
    CHECK_INT(cterm__cell_at(b, 3, 0)->ch, 0);

    /* No room for the right half: draw a space rather than half a character. */
    cterm_clear(b);
    cterm_text(b, 5, 0, cterm_plain(), zhong);
    CHECK_INT(cterm__cell_at(b, 5, 0)->ch, ' ');

    /* Left half off-screen: same idea. */
    cterm_clear(b);
    cterm_text(b, -1, 0, cterm_plain(), zhong);
    CHECK_INT(cterm__cell_at(b, 0, 0)->ch, ' ');
    cterm_buffer_free(b);
}

static void test_combining_marks(void) {
    cterm_buffer *b = cterm_buffer_new(5, 1);
    const cterm_cell *cell;

    /* Bengali KU: the consonant KA plus a vowel sign that sits on it. */
    CHECK_INT(cterm_text(b, 0, 0, cterm_plain(), "\xE0\xA6\x95\xE0\xA7\x81x"), 2);
    cell = cterm__cell_at(b, 0, 0);
    CHECK_INT(cell->ch, 0x995);
    CHECK_INT(cell->marks[0], 0x9C1);
    CHECK_INT(cterm__cell_at(b, 1, 0)->ch, 'x');

    /* A mark at the start of a string has nothing to attach to. */
    cterm_clear(b);
    cterm_text(b, 1, 0, cterm_plain(), "a");
    CHECK_INT(cterm_text(b, 2, 0, cterm_plain(), "\xCC\x81"), 2);
    CHECK_INT(cterm__cell_at(b, 1, 0)->marks[0], 0);

    /* cterm_put with a mark attaches to the cell on its left. */
    cterm_put(b, 2, 0, 0x301, cterm_plain());
    CHECK_INT(cterm__cell_at(b, 1, 0)->marks[0], 0x301);

    /* Marks attach to the left half of a wide character. */
    cterm_clear(b);
    cterm_text(b, 0, 0, cterm_plain(), "\xE4\xB8\xAD\xCC\x81");
    CHECK_INT(cterm__cell_at(b, 0, 0)->marks[0], 0x301);

    /* Marks past the limit are dropped safely. */
    cterm_clear(b);
    cterm_text(b, 0, 0, cterm_plain(), "e\xCC\x81\xCC\x82\xCC\x83\xCC\x84\xCC\x85\xCC\x86");
    CHECK_INT(cterm__cell_at(b, 0, 0)->marks[CTERM_MAX_MARKS - 1], 0x301 + CTERM_MAX_MARKS - 1);

    /* Overwriting a cell removes its marks. */
    cterm_put(b, 0, 0, 'z', cterm_plain());
    CHECK_INT(cterm__cell_at(b, 0, 0)->marks[0], 0);
    cterm_buffer_free(b);
}

static void test_clip(void) {
    cterm_buffer *b = cterm_buffer_new(6, 3);
    cterm_rect old = cterm_set_clip(b, cterm_rect_make(1, 1, 3, 1));
    CHECK_INT(old.w, 6);

    cterm_text(b, 0, 1, cterm_plain(), "abcdef");
    cterm_text(b, 0, 0, cterm_plain(), "zzz");
    cterm_text(b, 0, 2, cterm_plain(), "zzz");
    CHECK_STR(line(b, 0), "      ");
    CHECK_STR(line(b, 1), " bcd  ");
    CHECK_STR(line(b, 2), "      ");

    /* A wide character crossing the clip edge turns into a space. */
    cterm_text(b, 3, 1, cterm_plain(), "\xE4\xB8\xAD");
    CHECK_INT(cterm__cell_at(b, 3, 1)->ch, ' ');
    CHECK_INT(cterm__cell_at(b, 4, 1)->ch, ' ');

    /* A clip area can't extend past the buffer. */
    cterm_set_clip(b, cterm_rect_make(-5, -5, 100, 100));
    CHECK_INT(b->clip.x, 0);
    CHECK_INT(b->clip.w, 6);

    /* Clearing removes the clip. */
    cterm_set_clip(b, cterm_rect_make(0, 0, 1, 1));
    cterm_clear(b);
    CHECK_INT(b->clip.w, 6);
    cterm_buffer_free(b);
}

static void test_fill(void) {
    cterm_buffer *b = cterm_buffer_new(5, 3);
    cterm_fill(b, cterm_rect_make(1, 1, 3, 2), '#', cterm_plain());
    CHECK_STR(line(b, 0), "     ");
    CHECK_STR(line(b, 1), " ### ");
    CHECK_STR(line(b, 2), " ### ");

    /* A rectangle partly off the buffer is handled. */
    cterm_clear(b);
    cterm_fill(b, cterm_rect_make(-2, -2, 4, 3), '#', cterm_plain());
    CHECK_STR(line(b, 0), "##   ");
    CHECK_STR(line(b, 1), "     ");

    /* Wide characters fill in steps of two. */
    cterm_clear(b);
    cterm_fill(b, cterm_rect_make(0, 0, 5, 1), 0x4E2D, cterm_plain());
    CHECK_INT(cterm__cell_at(b, 0, 0)->ch, 0x4E2D);
    CHECK_INT(cterm__cell_at(b, 2, 0)->ch, 0x4E2D);
    CHECK_INT(cterm__cell_at(b, 4, 0)->ch, ' ');

    /* A mark has no width, so it fills nothing, and must not hang. */
    cterm_clear(b);
    cterm_fill(b, cterm_rect_make(0, 0, 5, 1), 0x301, cterm_plain());
    CHECK_STR(line(b, 0), "     ");
    cterm_buffer_free(b);
}

static void test_rect_helpers(void) {
    cterm_rect r = cterm_rect_intersect(cterm_rect_make(0, 0, 5, 5), cterm_rect_make(3, 3, 5, 5));
    CHECK_INT(r.x, 3);
    CHECK_INT(r.w, 2);
    r = cterm_rect_intersect(cterm_rect_make(0, 0, 2, 2), cterm_rect_make(5, 5, 2, 2));
    CHECK_INT(r.w, 0);
    CHECK_INT(r.h, 0);
    CHECK(cterm_rect_contains(cterm_rect_make(1, 1, 2, 2), 1, 1));
    CHECK(cterm_rect_contains(cterm_rect_make(1, 1, 2, 2), 2, 2));
    CHECK(!cterm_rect_contains(cterm_rect_make(1, 1, 2, 2), 3, 1));
    CHECK(!cterm_rect_contains(cterm_rect_make(1, 1, 2, 2), 0, 1));
    r = cterm_rect_inset(cterm_rect_make(0, 0, 10, 4), 1);
    CHECK_INT(r.x, 1);
    CHECK_INT(r.w, 8);
    CHECK_INT(r.h, 2);
    r = cterm_rect_inset(cterm_rect_make(0, 0, 1, 1), 5);
    CHECK_INT(r.w, 0);
}

static void test_line_respects_capacity(void) {
    cterm_buffer *b = cterm_buffer_new(5, 1);
    char small[4];
    cterm_text(b, 0, 0, cterm_plain(), "abcde");
    CHECK_INT(cterm_buffer_line(b, 0, small, (int)sizeof small), 3);
    CHECK_STR(small, "abc");
    cterm_buffer_free(b);
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
