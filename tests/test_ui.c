/* Tests for layout and widgets. No terminal needed: widgets draw on an off-screen
 * buffer and get scripted events, the same way your own app's UI can be tested. */
#define CTERM_IMPLEMENTATION
#include "cterm.h"
#include "test.h"

/* --- Helpers ------------------------------------------------------------- */

static const char *line(const cterm_buffer *b, int y) {
    static char text[2048];
    cterm_buffer_line(b, y, text, (int)sizeof text);
    return text;
}

static cterm_event key_event(int key, int mods) {
    cterm_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = CTERM_EVENT_KEY;
    ev.key = key;
    ev.mods = mods;
    return ev;
}

static cterm_event mouse(cterm_mouse_action action, int x, int y) {
    cterm_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = CTERM_EVENT_MOUSE;
    ev.action = action;
    ev.button = action == CTERM_MOUSE_PRESS || action == CTERM_MOUSE_DRAG || action == CTERM_MOUSE_RELEASE
                    ? CTERM_BUTTON_LEFT : CTERM_BUTTON_NONE;
    ev.x = x;
    ev.y = y;
    return ev;
}

static cterm_event paste(const char *text) {
    cterm_event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = CTERM_EVENT_PASTE;
    ev.text = text;
    ev.text_length = strlen(text);
    return ev;
}

/* --- Layout -------------------------------------------------------------- */

#define COUNT(array) ((int)(sizeof(array) / sizeof((array)[0])))

static void test_split_fixed_and_fill(void) {
    int sizes[] = { CTERM_FIXED(1), CTERM_FILL(1), CTERM_FIXED(2) };
    cterm_rect out[3];
    cterm_split_rows(cterm_rect_make(5, 10, 40, 20), 0, sizes, COUNT(sizes), out);
    CHECK_INT(out[0].y, 10);
    CHECK_INT(out[0].h, 1);
    CHECK_INT(out[1].y, 11);
    CHECK_INT(out[1].h, 17);
    CHECK_INT(out[2].y, 28);
    CHECK_INT(out[2].h, 2);
    /* The other axis is unchanged. */
    CHECK_INT(out[1].x, 5);
    CHECK_INT(out[1].w, 40);
}

static void test_split_columns(void) {
    int sizes[] = { CTERM_FIXED(10), CTERM_FILL(1) };
    cterm_rect out[2];
    cterm_split_columns(cterm_rect_make(0, 3, 30, 5), 0, sizes, COUNT(sizes), out);
    CHECK_INT(out[0].x, 0);
    CHECK_INT(out[0].w, 10);
    CHECK_INT(out[1].x, 10);
    CHECK_INT(out[1].w, 20);
    CHECK_INT(out[1].y, 3);
    CHECK_INT(out[1].h, 5);
}

static void test_split_percent(void) {
    int sizes[] = { CTERM_PERCENT(25), CTERM_FILL(1) };
    cterm_rect out[2];
    cterm_split_columns(cterm_rect_make(0, 0, 100, 1), 0, sizes, COUNT(sizes), out);
    CHECK_INT(out[0].w, 25);
    CHECK_INT(out[1].w, 75);
}

static void test_split_weights_and_rounding(void) {
    /* Three equal shares of 10: nothing lost to rounding, nothing overlapping. */
    int equal[] = { CTERM_FILL(1), CTERM_FILL(1), CTERM_FILL(1) };
    int weighted[] = { CTERM_FILL(1), CTERM_FILL(3) };
    cterm_rect out[3];
    cterm_split_columns(cterm_rect_make(0, 0, 10, 1), 0, equal, 3, out);
    CHECK_INT(out[0].w, 4);
    CHECK_INT(out[1].x, 4);
    CHECK_INT(out[1].w, 3);
    CHECK_INT(out[2].x, 7);
    CHECK_INT(out[2].w, 3);

    cterm_split_columns(cterm_rect_make(0, 0, 40, 1), 0, weighted, 2, out);
    CHECK_INT(out[0].w, 10);
    CHECK_INT(out[1].w, 30);
}

static void test_split_gap(void) {
    int sizes[] = { CTERM_FILL(1), CTERM_FILL(1) };
    cterm_rect out[2];
    cterm_split_columns(cterm_rect_make(0, 0, 11, 1), 1, sizes, 2, out);
    CHECK_INT(out[0].x, 0);
    CHECK_INT(out[0].w, 5);
    CHECK_INT(out[1].x, 6);
    CHECK_INT(out[1].w, 5);
}

static void test_split_not_enough_room(void) {
    int sizes[] = { CTERM_FIXED(5), CTERM_FIXED(5), CTERM_FILL(1) };
    cterm_rect out[3];
    cterm_split_columns(cterm_rect_make(0, 0, 7, 1), 0, sizes, 3, out);
    CHECK_INT(out[0].w, 5);
    CHECK_INT(out[1].w, 2);
    CHECK_INT(out[2].w, 0);

    /* An area of nothing, and a gap bigger than the area, must both be safe. */
    cterm_split_columns(cterm_rect_make(0, 0, 0, 0), 0, sizes, 3, out);
    CHECK_INT(out[0].w, 0);
    cterm_split_columns(cterm_rect_make(0, 0, 3, 1), 10, sizes, 3, out);
    CHECK_INT(out[0].w, 0);
    CHECK_INT(out[2].w, 0);
}

static void test_split_never_overflows(void) {
    /* Whatever the sizes, children must stay inside the parent without overlap. */
    uint32_t state = 7;
    int run;
    for (run = 0; run < 3000; run++) {
        int sizes[6], i, count, extent, gap, end;
        cterm_rect out[6];
        state = state * 1664525u + 1013904223u;
        count = (int)((state >> 16) % 6) + 1;
        extent = (int)((state >> 8) % 60);
        gap = (int)((state >> 4) % 3);
        for (i = 0; i < count; i++) {
            state = state * 1664525u + 1013904223u;
            switch ((state >> 20) % 3) {
                case 0:  sizes[i] = CTERM_FIXED((int)((state >> 8) % 30)); break;
                case 1:  sizes[i] = CTERM_PERCENT((int)((state >> 8) % 101)); break;
                default: sizes[i] = CTERM_FILL((int)((state >> 8) % 4) + 1); break;
            }
        }
        cterm_split_columns(cterm_rect_make(0, 0, extent, 1), gap, sizes, count, out);
        end = 0;
        for (i = 0; i < count; i++) {
            CHECK(out[i].w >= 0);
            CHECK(out[i].x >= end);
            if (out[i].w > 0) end = out[i].x + out[i].w;
            CHECK(end <= extent);
        }
    }
}

/* --- Text input ---------------------------------------------------------- */

/* A one-widget test harness: draws an input and sends it events. */
typedef struct input_rig {
    cterm_ui ui;
    cterm_buffer *b;
    cterm_input input;
    char text[64];
    int result;
} input_rig;

static void rig_init(input_rig *r, int width) {
    cterm_ui_init(&r->ui);
    r->b = cterm_buffer_new(width, 1);
    cterm_input_init(&r->input, r->text, sizeof r->text);
    r->result = 0;
}

static void rig_frame(input_rig *r, const cterm_event *ev) {
    cterm_clear(r->b);
    cterm_ui_begin(&r->ui, r->b, ev);
    r->result = cterm_ui_input(&r->ui, cterm_bounds(r->b), &r->input);
    cterm_ui_end(&r->ui);
}

static void rig_key(input_rig *r, int key, int mods) {
    cterm_event ev = key_event(key, mods);
    rig_frame(r, &ev);
}

static void rig_type(input_rig *r, const char *utf8) {
    size_t n = strlen(utf8);
    const unsigned char *s = (const unsigned char *)utf8;
    while (n > 0) {
        uint32_t cp = 0;
        int used = cterm__utf8_decode(s, n, &cp);
        rig_key(r, (int)cp, 0);
        s += used;
        n -= (size_t)used;
    }
}

static void test_input_typing(void) {
    input_rig r;
    rig_init(&r, 20);
    rig_type(&r, "hello");
    CHECK_STR(r.text, "hello");
    CHECK_INT(r.input.cursor, 5);
    CHECK(r.result & CTERM_UI_CHANGED);
    CHECK_STR(line(r.b, 0), "hello               ");

    /* The terminal cursor sits just after the text. */
    CHECK_INT(r.b->cursor_visible, 1);
    CHECK_INT(r.b->cursor_x, 5);
    cterm_buffer_free(r.b);
}

static void test_input_editing(void) {
    input_rig r;
    rig_init(&r, 20);
    rig_type(&r, "abc");

    rig_key(&r, CTERM_KEY_LEFT, 0);
    CHECK_INT(r.input.cursor, 2);
    rig_type(&r, "X");
    CHECK_STR(r.text, "abXc");

    rig_key(&r, CTERM_KEY_BACKSPACE, 0);
    CHECK_STR(r.text, "abc");
    CHECK_INT(r.input.cursor, 2);

    rig_key(&r, CTERM_KEY_DELETE, 0);
    CHECK_STR(r.text, "ab");

    rig_key(&r, CTERM_KEY_HOME, 0);
    CHECK_INT(r.input.cursor, 0);
    rig_key(&r, CTERM_KEY_BACKSPACE, 0); /* nothing to delete */
    CHECK_STR(r.text, "ab");
    CHECK(!(r.result & CTERM_UI_CHANGED));
    rig_key(&r, CTERM_KEY_LEFT, 0);      /* can't go further */
    CHECK_INT(r.input.cursor, 0);

    rig_key(&r, CTERM_KEY_END, 0);
    CHECK_INT(r.input.cursor, 2);
    rig_key(&r, CTERM_KEY_RIGHT, 0);
    CHECK_INT(r.input.cursor, 2);
    rig_key(&r, CTERM_KEY_DELETE, 0);    /* nothing to delete */
    CHECK_STR(r.text, "ab");

    rig_key(&r, 'a', CTERM_MOD_CTRL);
    CHECK_INT(r.input.cursor, 0);
    rig_key(&r, 'e', CTERM_MOD_CTRL);
    CHECK_INT(r.input.cursor, 2);
    cterm_buffer_free(r.b);
}

static void test_input_word_operations(void) {
    input_rig r;
    rig_init(&r, 40);
    rig_type(&r, "one two  three");

    rig_key(&r, CTERM_KEY_LEFT, CTERM_MOD_CTRL);
    CHECK_INT(r.input.cursor, 9);   /* start of "three" */
    rig_key(&r, CTERM_KEY_LEFT, CTERM_MOD_CTRL);
    CHECK_INT(r.input.cursor, 4);   /* start of "two", past both spaces */
    rig_key(&r, CTERM_KEY_RIGHT, CTERM_MOD_CTRL);
    CHECK_INT(r.input.cursor, 7);   /* end of "two" */
    rig_key(&r, 'b', CTERM_MOD_ALT);
    CHECK_INT(r.input.cursor, 4);
    rig_key(&r, 'f', CTERM_MOD_ALT);
    CHECK_INT(r.input.cursor, 7);

    rig_key(&r, CTERM_KEY_END, 0);
    rig_key(&r, 'w', CTERM_MOD_CTRL);
    CHECK_STR(r.text, "one two  ");
    rig_key(&r, CTERM_KEY_BACKSPACE, CTERM_MOD_ALT);
    CHECK_STR(r.text, "one ");

    rig_key(&r, CTERM_KEY_HOME, 0);
    rig_key(&r, CTERM_KEY_DELETE, CTERM_MOD_CTRL);
    CHECK_STR(r.text, " ");

    cterm_input_set(&r.input, "abc def");
    r.input.cursor = 3;
    rig_key(&r, 'k', CTERM_MOD_CTRL);
    CHECK_STR(r.text, "abc");
    cterm_input_set(&r.input, "abc def");
    r.input.cursor = 4;
    rig_key(&r, 'u', CTERM_MOD_CTRL);
    CHECK_STR(r.text, "def");
    CHECK_INT(r.input.cursor, 0);
    cterm_buffer_free(r.b);
}

static void test_input_unicode(void) {
    input_rig r;
    rig_init(&r, 20);

    /* Wide characters: the cursor counts cells, not bytes or characters. */
    rig_type(&r, "\xE4\xB8\xAD\xE6\x96\x87");
    CHECK_INT(r.input.cursor, 6);
    CHECK_INT(r.b->cursor_x, 4);
    rig_key(&r, CTERM_KEY_LEFT, 0);
    CHECK_INT(r.input.cursor, 3);
    CHECK_INT(r.b->cursor_x, 2);
    rig_key(&r, CTERM_KEY_BACKSPACE, 0);
    CHECK_STR(r.text, "\xE6\x96\x87");
    CHECK_INT(r.input.cursor, 0);
    cterm_buffer_free(r.b);

    /* Bengali KU: arrows jump the whole cluster, and Backspace takes back the sign. */
    rig_init(&r, 20);
    rig_type(&r, "\xE0\xA6\x95\xE0\xA7\x81");
    CHECK_INT(r.input.cursor, 6);
    CHECK_INT(r.b->cursor_x, 1);
    rig_key(&r, CTERM_KEY_LEFT, 0);
    CHECK_INT(r.input.cursor, 0);
    rig_key(&r, CTERM_KEY_RIGHT, 0);
    CHECK_INT(r.input.cursor, 6);
    rig_key(&r, CTERM_KEY_BACKSPACE, 0);
    CHECK_STR(r.text, "\xE0\xA6\x95");

    /* Delete removes the whole cluster at once. */
    cterm_input_set(&r.input, "\xE0\xA6\x95\xE0\xA7\x81x");
    r.input.cursor = 0;
    rig_key(&r, CTERM_KEY_DELETE, 0);
    CHECK_STR(r.text, "x");
    cterm_buffer_free(r.b);
}

static void test_input_capacity(void) {
    input_rig r;
    char small[5];
    rig_init(&r, 20);
    cterm_input_init(&r.input, small, sizeof small);

    rig_type(&r, "abcdef");
    CHECK_STR(small, "abcd");
    CHECK_INT(strlen(small), 4);

    /* A multi-byte character that doesn't fit is refused whole. */
    cterm_input_set(&r.input, "abc");
    rig_type(&r, "\xE4\xB8\xAD");
    CHECK_STR(small, "abc");
    CHECK(!(r.result & CTERM_UI_CHANGED));

    /* cterm_input_set cuts on a character boundary too. */
    cterm_input_set(&r.input, "ab\xE4\xB8\xAD");
    CHECK_STR(small, "ab");
    cterm_buffer_free(r.b);

    /* A capacity of 1 only has room for the NUL, and no storage is also safe. */
    rig_init(&r, 20);
    cterm_input_init(&r.input, small, 1);
    rig_type(&r, "a");
    CHECK_STR(small, "");
    cterm_input_init(&r.input, NULL, 0);
    rig_type(&r, "a");
    cterm_input_set(&r.input, "x");
    cterm_buffer_free(r.b);
}

static void test_input_paste(void) {
    input_rig r;
    cterm_event ev;
    rig_init(&r, 40);

    ev = paste("hello");
    rig_frame(&r, &ev);
    CHECK_STR(r.text, "hello");
    CHECK(r.result & CTERM_UI_CHANGED);
    CHECK(cterm_ui_event_handled(&r.ui));

    /* It's a one-line field: line breaks turn into spaces. */
    cterm_input_set(&r.input, "");
    ev = paste("one\ntwo\r\nthree\tfour");
    rig_frame(&r, &ev);
    CHECK_STR(r.text, "one two three four");

    /* Escape sequences in a paste must not end up in the text as control codes. */
    cterm_input_set(&r.input, "");
    ev = paste("a\x1b[31mb\x7f\xC2\x9B" "c");
    rig_frame(&r, &ev);
    CHECK_STR(r.text, "a[31mbc");

    /* Invalid UTF-8 is dropped, but a real U+FFFD is kept. */
    cterm_input_set(&r.input, "");
    ev = paste("x\xFFy\xEF\xBF\xBDz");
    rig_frame(&r, &ev);
    CHECK_STR(r.text, "xy\xEF\xBF\xBDz");
    cterm_buffer_free(r.b);
}

static void test_input_submit_and_unknown_keys(void) {
    input_rig r;
    rig_init(&r, 20);
    rig_type(&r, "x");

    rig_key(&r, CTERM_KEY_ENTER, 0);
    CHECK(r.result & CTERM_UI_SUBMITTED);
    CHECK(!(r.result & CTERM_UI_CHANGED));
    CHECK(cterm_ui_event_handled(&r.ui));
    CHECK_STR(r.text, "x");

    /* Keys the input doesn't use go back to the app. */
    rig_key(&r, CTERM_KEY_F5, 0);
    CHECK(!cterm_ui_event_handled(&r.ui));
    rig_key(&r, 'q', CTERM_MOD_CTRL);
    CHECK(!cterm_ui_event_handled(&r.ui));
    CHECK_STR(r.text, "x");

    /* Ctrl+letter and Alt+letter shouldn't type the letter. */
    rig_key(&r, 'z', CTERM_MOD_ALT);
    CHECK_STR(r.text, "x");
    cterm_buffer_free(r.b);
}

static void test_input_horizontal_scroll(void) {
    input_rig r;
    rig_init(&r, 5);
    rig_type(&r, "abcdefgh");
    /* The cursor needs a cell, so 4 characters show, and the cursor is on the last. */
    CHECK_STR(line(r.b, 0), "efgh ");
    CHECK_INT(r.b->cursor_x, 4);

    rig_key(&r, CTERM_KEY_HOME, 0);
    CHECK_STR(line(r.b, 0), "abcde");
    CHECK_INT(r.b->cursor_x, 0);

    rig_key(&r, CTERM_KEY_END, 0);
    CHECK_STR(line(r.b, 0), "efgh ");

    /* Deleting a lot pulls the view back, so the field isn't left empty. */
    rig_key(&r, 'u', CTERM_MOD_CTRL);
    CHECK_INT(r.input.scroll, 0);
    CHECK_INT(r.b->cursor_x, 0);
    cterm_buffer_free(r.b);

    /* A wide character crossing the left edge turns into a blank. */
    rig_init(&r, 4);
    rig_type(&r, "\xE4\xB8\xAD\xE6\x96\x87\xE5\xAD\x97");
    CHECK_INT(r.b->cursor_x, 3);
    cterm_buffer_free(r.b);
}

static void test_input_mouse(void) {
    input_rig r;
    cterm_event ev;
    rig_init(&r, 20);
    rig_type(&r, "abcdef");

    ev = mouse(CTERM_MOUSE_PRESS, 2, 0);
    rig_frame(&r, &ev);
    CHECK_INT(r.input.cursor, 2);

    ev = mouse(CTERM_MOUSE_PRESS, 15, 0); /* past the end */
    rig_frame(&r, &ev);
    CHECK_INT(r.input.cursor, 6);

    /* Clicking on either half of a wide character lands before it. */
    cterm_input_set(&r.input, "\xE4\xB8\xAD\xE6\x96\x87");
    rig_frame(&r, NULL);
    ev = mouse(CTERM_MOUSE_PRESS, 3, 0);
    rig_frame(&r, &ev);
    CHECK_INT(r.input.cursor, 3);
    ev = mouse(CTERM_MOUSE_PRESS, 2, 0);
    rig_frame(&r, &ev);
    CHECK_INT(r.input.cursor, 3);
    cterm_buffer_free(r.b);
}

static void test_input_placeholder_and_mask(void) {
    input_rig r;
    rig_init(&r, 10);
    r.input.placeholder = "Search";
    rig_frame(&r, NULL);
    CHECK_STR(line(r.b, 0), "Search    ");
    CHECK_INT(r.b->cursor_x, 0);
    rig_type(&r, "x");
    CHECK_STR(line(r.b, 0), "x         ");
    cterm_buffer_free(r.b);

    rig_init(&r, 10);
    r.input.mask = '*';
    rig_type(&r, "pass");
    CHECK_STR(r.text, "pass");
    CHECK_STR(line(r.b, 0), "****      ");
    CHECK_INT(r.b->cursor_x, 4);

    /* A mask hides how wide the real characters are, and marks are invisible. */
    cterm_input_set(&r.input, "\xE4\xB8\xAD" "e\xCC\x81");
    rig_frame(&r, NULL);
    CHECK_STR(line(r.b, 0), "**        ");
    CHECK_INT(r.b->cursor_x, 2);
    cterm_buffer_free(r.b);
}

static void test_input_survives_app_changes(void) {
    /* The app can change the text directly, leaving the cursor pointing anywhere. */
    input_rig r;
    rig_init(&r, 20);
    rig_type(&r, "hello");
    strcpy(r.text, "hi");
    rig_frame(&r, NULL);
    CHECK_INT(r.input.cursor, 2);

    /* Or pointing into the middle of a multi-byte character. */
    strcpy(r.text, "\xE4\xB8\xAD");
    r.input.cursor = 1;
    rig_frame(&r, NULL);
    CHECK_INT(r.input.cursor, 0);
    cterm_buffer_free(r.b);
}

static void test_input_stays_valid_under_fuzz(void) {
    /* Random keys in random order: the text must always stay valid UTF-8, the cursor
     * must stay on a boundary, and nothing may overflow. Sanitizers do the rest. */
    static const int keys[] = {
        CTERM_KEY_LEFT, CTERM_KEY_RIGHT, CTERM_KEY_HOME, CTERM_KEY_END, CTERM_KEY_BACKSPACE,
        CTERM_KEY_DELETE, 'a', 'z', ' ', 0xE9, 0x995, 0x9C1, 0x4E2D, 0x1F600, 0x301
    };
    static const int mods[] = { 0, 0, 0, CTERM_MOD_CTRL, CTERM_MOD_ALT };
    uint32_t state = 42;
    input_rig r;
    int i;
    rig_init(&r, 8);
    for (i = 0; i < 20000; i++) {
        size_t n, at;
        state = state * 1664525u + 1013904223u;
        rig_key(&r, keys[(state >> 16) % COUNT(keys)], mods[(state >> 8) % COUNT(mods)]);
        n = strlen(r.text);
        CHECK(n < sizeof r.text);
        CHECK(r.input.cursor <= n);
        CHECK(r.input.cursor == n || ((unsigned char)r.text[r.input.cursor] & 0xC0) != 0x80);
        for (at = 0; at < n;) {
            uint32_t cp = 0;
            int used = cterm__utf8_decode((const unsigned char *)r.text + at, n - at, &cp);
            CHECK(used > 0);
            if (used <= 0 || (cp == CTERM__REPLACEMENT && used == 1)) {
                CHECK(0);
                break;
            }
            at += (size_t)used;
        }
        if (r.b->cursor_visible) CHECK(r.b->cursor_x >= 0 && r.b->cursor_x < r.b->width);
    }
    cterm_buffer_free(r.b);
}

/* --- List ---------------------------------------------------------------- */

static const char *const fruit[] = {
    "apple", "banana", "cherry", "date", "elder", "fig", "grape", "honey", "iris", "jade"
};

typedef struct list_rig {
    cterm_ui ui;
    cterm_buffer *b;
    cterm_list list;
    int count;
    int result;
} list_rig;

static void list_rig_init(list_rig *r, int width, int height, int count) {
    cterm_ui_init(&r->ui);
    r->b = cterm_buffer_new(width, height);
    memset(&r->list, 0, sizeof r->list);
    r->count = count;
    r->result = 0;
}

static void list_frame(list_rig *r, const cterm_event *ev) {
    cterm_clear(r->b);
    cterm_ui_begin(&r->ui, r->b, ev);
    r->result = cterm_ui_list(&r->ui, cterm_bounds(r->b), &r->list, fruit, r->count);
    cterm_ui_end(&r->ui);
}

static void list_key(list_rig *r, int key) {
    cterm_event ev = key_event(key, 0);
    list_frame(r, &ev);
}

static void list_mouse(list_rig *r, cterm_mouse_action action, int x, int y) {
    cterm_event ev = mouse(action, x, y);
    list_frame(r, &ev);
}

static void test_list_draws_rows(void) {
    list_rig r;
    list_rig_init(&r, 8, 3, 3);
    list_frame(&r, NULL);
    CHECK_STR(line(r.b, 0), "apple   ");
    CHECK_STR(line(r.b, 1), "banana  ");
    CHECK_STR(line(r.b, 2), "cherry  ");
    /* The selected row is highlighted across its full width. */
    CHECK(cterm__cell_at(r.b, 7, 0)->style.attrs & CTERM_REVERSE);
    CHECK(!(cterm__cell_at(r.b, 7, 1)->style.attrs & CTERM_REVERSE));
    cterm_buffer_free(r.b);
}

static void test_list_keys(void) {
    list_rig r;
    list_rig_init(&r, 8, 3, 10);
    list_frame(&r, NULL);

    list_key(&r, CTERM_KEY_DOWN);
    CHECK_INT(r.list.selected, 1);
    CHECK(r.result & CTERM_UI_CHANGED);
    CHECK(cterm_ui_event_handled(&r.ui));

    list_key(&r, CTERM_KEY_UP);
    CHECK_INT(r.list.selected, 0);
    list_key(&r, CTERM_KEY_UP);          /* already at the top */
    CHECK_INT(r.list.selected, 0);
    CHECK(!(r.result & CTERM_UI_CHANGED));

    list_key(&r, CTERM_KEY_END);
    CHECK_INT(r.list.selected, 9);
    list_key(&r, CTERM_KEY_DOWN);        /* already at the bottom */
    CHECK_INT(r.list.selected, 9);
    list_key(&r, CTERM_KEY_HOME);
    CHECK_INT(r.list.selected, 0);

    list_key(&r, CTERM_KEY_PAGE_DOWN);   /* one row less than the height */
    CHECK_INT(r.list.selected, 2);
    list_key(&r, CTERM_KEY_PAGE_UP);
    CHECK_INT(r.list.selected, 0);

    list_key(&r, CTERM_KEY_ENTER);
    CHECK(r.result & CTERM_UI_SUBMITTED);

    list_key(&r, 'x');                   /* not ours: goes back to the app */
    CHECK(!cterm_ui_event_handled(&r.ui));
    cterm_buffer_free(r.b);
}

static void test_list_scrolls_with_selection(void) {
    list_rig r;
    list_rig_init(&r, 8, 3, 10);
    list_frame(&r, NULL);
    CHECK_INT(r.list.scroll, 0);

    list_key(&r, CTERM_KEY_DOWN);
    list_key(&r, CTERM_KEY_DOWN);
    CHECK_INT(r.list.scroll, 0);
    list_key(&r, CTERM_KEY_DOWN);
    CHECK_INT(r.list.selected, 3);
    CHECK_INT(r.list.scroll, 1);
    CHECK(strncmp(line(r.b, 0), "banana", 6) == 0);
    CHECK(strncmp(line(r.b, 2), "date", 4) == 0);

    list_key(&r, CTERM_KEY_END);
    CHECK_INT(r.list.scroll, 7);
    CHECK(strncmp(line(r.b, 2), "jade", 4) == 0);
    list_key(&r, CTERM_KEY_HOME);
    CHECK_INT(r.list.scroll, 0);

    /* The app sets the selection itself: the list scrolls to show it. */
    r.list.selected = 8;
    list_frame(&r, NULL);
    CHECK_INT(r.list.scroll, 6);
    cterm_buffer_free(r.b);
}

static void test_list_mouse(void) {
    list_rig r;
    list_rig_init(&r, 8, 3, 10);
    list_frame(&r, NULL);

    list_mouse(&r, CTERM_MOUSE_PRESS, 2, 2);
    CHECK_INT(r.list.selected, 2);
    CHECK(r.result & CTERM_UI_CHANGED);

    /* The wheel scrolls without moving the selection, and doesn't jump back. */
    list_mouse(&r, CTERM_MOUSE_SCROLL_DOWN, 2, 1);
    CHECK_INT(r.list.scroll, 3);
    CHECK_INT(r.list.selected, 2);
    list_frame(&r, NULL);
    CHECK_INT(r.list.scroll, 3);

    list_mouse(&r, CTERM_MOUSE_SCROLL_DOWN, 2, 1);
    list_mouse(&r, CTERM_MOUSE_SCROLL_DOWN, 2, 1);
    CHECK_INT(r.list.scroll, 7); /* stops at the end */
    list_mouse(&r, CTERM_MOUSE_SCROLL_UP, 2, 1);
    CHECK_INT(r.list.scroll, 4);

    /* A click on a scrolled list picks the row that's shown there. */
    list_mouse(&r, CTERM_MOUSE_PRESS, 2, 1);
    CHECK_INT(r.list.selected, 5);

    /* A click outside does nothing. */
    {
        cterm_buffer *big = cterm_buffer_new(20, 20);
        cterm_event ev = mouse(CTERM_MOUSE_PRESS, 15, 15);
        cterm_ui_begin(&r.ui, big, &ev);
        cterm_ui_list(&r.ui, cterm_rect_make(0, 0, 8, 3), &r.list, fruit, 10);
        CHECK(!cterm_ui_event_handled(&r.ui));
        cterm_ui_end(&r.ui);
        cterm_buffer_free(big);
    }
    cterm_buffer_free(r.b);
}

static void test_list_empty_rows_and_clicks(void) {
    list_rig r;
    list_rig_init(&r, 8, 5, 2);
    list_frame(&r, NULL);
    /* Clicking under the last row doesn't select anything. */
    list_mouse(&r, CTERM_MOUSE_PRESS, 2, 4);
    CHECK_INT(r.list.selected, 0);
    cterm_buffer_free(r.b);

    /* No items at all. */
    list_rig_init(&r, 8, 3, 0);
    list_frame(&r, NULL);
    list_key(&r, CTERM_KEY_DOWN);
    list_key(&r, CTERM_KEY_ENTER);
    CHECK_INT(r.result, 0);
    CHECK_INT(r.list.selected, 0);
    CHECK_STR(line(r.b, 0), "        ");
    cterm_buffer_free(r.b);
}

static void test_list_count_shrinks(void) {
    list_rig r;
    list_rig_init(&r, 8, 3, 10);
    list_frame(&r, NULL);
    list_key(&r, CTERM_KEY_END);
    r.count = 2;
    list_frame(&r, NULL);
    CHECK_INT(r.list.selected, 1);
    CHECK_INT(r.list.scroll, 0);
    r.count = 0;
    list_frame(&r, NULL);
    CHECK_INT(r.list.selected, 0);
    cterm_buffer_free(r.b);
}

static void test_list_height_shrinks(void) {
    list_rig r;
    list_rig_init(&r, 8, 8, 10);
    list_frame(&r, NULL);
    r.list.selected = 7;
    list_frame(&r, NULL);
    CHECK_INT(r.list.scroll, 0);

    /* The terminal gets smaller: the selection must not disappear. */
    cterm_buffer_resize(r.b, 8, 3);
    list_frame(&r, NULL);
    CHECK_INT(r.list.scroll, 5);
    cterm_buffer_free(r.b);
}

static void test_list_scrollbar(void) {
    list_rig r;
    list_rig_init(&r, 8, 5, 10);
    list_frame(&r, NULL);
    /* The last column holds the bar: thumb at the top, then the track. */
    CHECK_INT(cterm__cell_at(r.b, 7, 0)->ch, 0x2588);
    CHECK_INT(cterm__cell_at(r.b, 7, 4)->ch, 0x2502);

    /* Clicking the bottom of the bar jumps to the end. */
    list_mouse(&r, CTERM_MOUSE_PRESS, 7, 4);
    CHECK_INT(r.list.scroll, 5);
    CHECK_INT(cterm__cell_at(r.b, 7, 4)->ch, 0x2588);
    CHECK_INT(r.list.selected, 0); /* moving the view doesn't change the selection */

    /* Dragging keeps following, even off the bar. */
    list_mouse(&r, CTERM_MOUSE_DRAG, 3, 0);
    CHECK_INT(r.list.scroll, 0);
    list_mouse(&r, CTERM_MOUSE_RELEASE, 3, 0);

    /* After releasing, moving over the bar no longer scrolls. */
    list_mouse(&r, CTERM_MOUSE_DRAG, 7, 4);
    CHECK_INT(r.list.scroll, 0);
    cterm_buffer_free(r.b);

    /* The wheel also works with the pointer over the bar. */
    list_rig_init(&r, 8, 5, 10);
    list_frame(&r, NULL);
    list_mouse(&r, CTERM_MOUSE_SCROLL_DOWN, 7, 2);
    CHECK_INT(r.list.scroll, 3);
    cterm_buffer_free(r.b);

    /* Text matters more than a bar: a one-column list doesn't get one. */
    list_rig_init(&r, 1, 2, 10);
    list_frame(&r, NULL);
    CHECK_STR(line(r.b, 0), "a");
    cterm_buffer_free(r.b);

    /* A list that fits has no bar, so text gets the whole width. */
    list_rig_init(&r, 6, 5, 2);
    list_frame(&r, NULL);
    CHECK_STR(line(r.b, 0), "apple ");
    CHECK_INT(cterm__cell_at(r.b, 5, 4)->ch, ' ');
    cterm_buffer_free(r.b);
}

static const char *numbered(int index, void *user_data) {
    static char text[32];
    int *calls = (int *)user_data;
    (*calls)++;
    snprintf(text, sizeof text, "row %d", index);
    return text;
}

static void test_list_callback_only_visible(void) {
    /* A million rows must cost the same as ten. */
    cterm_ui ui;
    cterm_list list;
    cterm_buffer *b = cterm_buffer_new(20, 4);
    int calls = 0;
    cterm_event ev = key_event(CTERM_KEY_END, 0);
    cterm_ui_init(&ui);
    memset(&list, 0, sizeof list);
    cterm_ui_begin(&ui, b, &ev);
    cterm_ui_list_fn(&ui, cterm_bounds(b), &list, 1000000, numbered, &calls);
    cterm_ui_end(&ui);
    CHECK_INT(calls, 4);
    CHECK_INT(list.selected, 999999);
    CHECK(strncmp(line(b, 3), "row 999999", 10) == 0);
    cterm_buffer_free(b);
}

static void test_list_long_text_is_clipped(void) {
    static const char *const items[] = { "this is far too long for the row" };
    cterm_ui ui;
    cterm_list list;
    cterm_buffer *b = cterm_buffer_new(6, 2);
    cterm_ui_init(&ui);
    memset(&list, 0, sizeof list);
    cterm_ui_begin(&ui, b, NULL);
    cterm_ui_list(&ui, cterm_bounds(b), &list, items, 1);
    cterm_ui_end(&ui);
    CHECK_STR(line(b, 0), "this i");
    CHECK_STR(line(b, 1), "      ");
    cterm_buffer_free(b);
}

/* --- Scrollable view ----------------------------------------------------- */

typedef struct scroll_rig {
    cterm_ui ui;
    cterm_buffer *b;
    cterm_scroll scroll;
    cterm_rect content;
    int content_width, content_height;
} scroll_rig;

static void scroll_rig_init(scroll_rig *r, int width, int height, int content_width, int content_height) {
    cterm_ui_init(&r->ui);
    r->b = cterm_buffer_new(width, height);
    memset(&r->scroll, 0, sizeof r->scroll);
    r->content_width = content_width;
    r->content_height = content_height;
}

static void scroll_frame(scroll_rig *r, const cterm_event *ev) {
    int i;
    cterm_clear(r->b);
    cterm_ui_begin(&r->ui, r->b, ev);
    r->content = cterm_ui_scroll_begin(&r->ui, cterm_bounds(r->b), &r->scroll, r->content_width, r->content_height);
    for (i = 0; i < r->content_height; i++) {
        cterm_textf(r->b, r->content.x, r->content.y + i, cterm_plain(), "L%02d-abcdefghij", i);
    }
    cterm_ui_scroll_end(&r->ui, &r->scroll);
    cterm_ui_end(&r->ui);
}

static void scroll_key(scroll_rig *r, int key) {
    cterm_event ev = key_event(key, 0);
    scroll_frame(r, &ev);
}

static void test_scroll_draws_visible_part(void) {
    scroll_rig r;
    scroll_rig_init(&r, 8, 3, 0, 10);
    scroll_frame(&r, NULL);
    /* 7 columns of text, then the scrollbar. */
    CHECK(strncmp(line(r.b, 0), "L00-abc", 7) == 0);
    CHECK(strncmp(line(r.b, 2), "L02-abc", 7) == 0);
    CHECK_INT(r.content.h, 10);
    CHECK_INT(cterm__cell_at(r.b, 7, 0)->ch, 0x2588);
    cterm_buffer_free(r.b);
}

static void test_scroll_keys(void) {
    scroll_rig r;
    scroll_rig_init(&r, 8, 3, 0, 10);
    scroll_frame(&r, NULL);

    scroll_key(&r, CTERM_KEY_DOWN);
    CHECK_INT(r.scroll.y, 1);
    CHECK(cterm_ui_event_handled(&r.ui));
    CHECK(strncmp(line(r.b, 0), "L01", 3) == 0);

    scroll_key(&r, CTERM_KEY_PAGE_DOWN);
    CHECK_INT(r.scroll.y, 3);
    scroll_key(&r, CTERM_KEY_END);
    CHECK_INT(r.scroll.y, 7);
    CHECK(strncmp(line(r.b, 2), "L09", 3) == 0);
    scroll_key(&r, CTERM_KEY_DOWN);    /* can't go past the end */
    CHECK_INT(r.scroll.y, 7);
    scroll_key(&r, CTERM_KEY_PAGE_UP);
    CHECK_INT(r.scroll.y, 5);
    scroll_key(&r, CTERM_KEY_HOME);
    CHECK_INT(r.scroll.y, 0);
    scroll_key(&r, CTERM_KEY_UP);      /* can't go above the start */
    CHECK_INT(r.scroll.y, 0);

    scroll_key(&r, 'x');               /* not ours */
    CHECK(!cterm_ui_event_handled(&r.ui));
    cterm_buffer_free(r.b);
}

static void test_scroll_horizontal(void) {
    scroll_rig r;
    scroll_rig_init(&r, 6, 3, 14, 3);
    scroll_frame(&r, NULL);
    CHECK_STR(line(r.b, 0), "L00-ab");

    scroll_key(&r, CTERM_KEY_RIGHT);
    CHECK_INT(r.scroll.x, 1);
    CHECK_STR(line(r.b, 0), "00-abc");
    scroll_key(&r, CTERM_KEY_LEFT);
    CHECK_STR(line(r.b, 0), "L00-ab");

    {
        cterm_event ev = mouse(CTERM_MOUSE_SCROLL_RIGHT, 1, 1);
        scroll_frame(&r, &ev);
        CHECK_INT(r.scroll.x, 3);
    }
    r.scroll.x = 500;
    scroll_frame(&r, NULL);
    CHECK_INT(r.scroll.x, 8);  /* limited to the end of the content */
    cterm_buffer_free(r.b);
}

static void test_scroll_mouse(void) {
    scroll_rig r;
    cterm_event ev;
    scroll_rig_init(&r, 8, 3, 0, 10);
    scroll_frame(&r, NULL);

    ev = mouse(CTERM_MOUSE_SCROLL_DOWN, 1, 1);
    scroll_frame(&r, &ev);
    CHECK_INT(r.scroll.y, 3);
    ev = mouse(CTERM_MOUSE_SCROLL_UP, 1, 1);
    scroll_frame(&r, &ev);
    CHECK_INT(r.scroll.y, 0);

    /* The scrollbar. */
    ev = mouse(CTERM_MOUSE_PRESS, 7, 2);
    scroll_frame(&r, &ev);
    CHECK_INT(r.scroll.y, 7);
    ev = mouse(CTERM_MOUSE_DRAG, 2, 0);
    scroll_frame(&r, &ev);
    CHECK_INT(r.scroll.y, 0);
    ev = mouse(CTERM_MOUSE_RELEASE, 2, 0);
    scroll_frame(&r, &ev);
    cterm_buffer_free(r.b);
}

static void test_scroll_content_fits(void) {
    scroll_rig r;
    cterm_event ev;
    scroll_rig_init(&r, 16, 5, 0, 2);
    scroll_frame(&r, NULL);
    /* No bar, the whole width for text, and no reason to accept keys. */
    CHECK_STR(line(r.b, 0), "L00-abcdefghij  ");
    CHECK_INT(cterm__cell_at(r.b, 15, 0)->ch, ' ');
    CHECK(!cterm_ui_has_focus(&r.ui, &r.scroll));

    ev = key_event(CTERM_KEY_DOWN, 0);
    scroll_frame(&r, &ev);
    CHECK_INT(r.scroll.y, 0);
    CHECK(!cterm_ui_event_handled(&r.ui));
    ev = mouse(CTERM_MOUSE_SCROLL_DOWN, 1, 1);
    scroll_frame(&r, &ev);
    CHECK_INT(r.scroll.y, 0);
    CHECK(!cterm_ui_event_handled(&r.ui));
    cterm_buffer_free(r.b);
}

static void test_scroll_clips_and_restores(void) {
    /* Drawing outside the view stays out, and the old clip comes back afterwards. */
    cterm_ui ui;
    cterm_scroll scroll;
    cterm_buffer *b = cterm_buffer_new(6, 3);
    cterm_rect c;
    cterm_ui_init(&ui);
    memset(&scroll, 0, sizeof scroll);
    cterm_ui_begin(&ui, b, NULL);
    c = cterm_ui_scroll_begin(&ui, cterm_rect_make(1, 1, 3, 1), &scroll, 0, 1);
    cterm_fill(b, cterm_bounds(b), '#', cterm_plain());
    CHECK_INT(c.x, 1);
    cterm_ui_scroll_end(&ui, &scroll);
    CHECK_STR(line(b, 0), "      ");
    CHECK_STR(line(b, 1), " ###  ");
    CHECK_STR(line(b, 2), "      ");
    CHECK_INT(b->clip.w, 6);
    cterm_ui_end(&ui);
    cterm_buffer_free(b);
}

static void test_scroll_shrinking_content(void) {
    scroll_rig r;
    cterm_event ev = key_event(CTERM_KEY_END, 0);
    scroll_rig_init(&r, 8, 3, 0, 10);
    scroll_frame(&r, NULL);
    scroll_frame(&r, &ev);
    CHECK_INT(r.scroll.y, 7);
    r.content_height = 4;
    scroll_frame(&r, NULL);
    CHECK_INT(r.scroll.y, 1);
    r.content_height = 0;
    scroll_frame(&r, NULL);
    CHECK_INT(r.scroll.y, 0);
    cterm_buffer_free(r.b);
}

/* --- Focus and several widgets together ---------------------------------- */

typedef struct form {
    cterm_ui ui;
    cterm_buffer *b;
    char text_a[16], text_b[16];
    cterm_input a, b_input;
    cterm_list list;
    int show_b;
} form;

static void form_init(form *f) {
    cterm_ui_init(&f->ui);
    f->b = cterm_buffer_new(20, 6);
    cterm_input_init(&f->a, f->text_a, sizeof f->text_a);
    cterm_input_init(&f->b_input, f->text_b, sizeof f->text_b);
    memset(&f->list, 0, sizeof f->list);
    f->show_b = 1;
}

static void form_frame(form *f, const cterm_event *ev) {
    cterm_clear(f->b);
    cterm_ui_begin(&f->ui, f->b, ev);
    cterm_ui_input(&f->ui, cterm_rect_make(0, 0, 20, 1), &f->a);
    if (f->show_b) cterm_ui_input(&f->ui, cterm_rect_make(0, 1, 20, 1), &f->b_input);
    cterm_ui_list(&f->ui, cterm_rect_make(0, 2, 20, 4), &f->list, fruit, 10);
    cterm_ui_end(&f->ui);
}

static void form_key(form *f, int key, int mods) {
    cterm_event ev = key_event(key, mods);
    form_frame(f, &ev);
}

static void test_focus_starts_on_first_widget(void) {
    form f;
    form_init(&f);
    form_frame(&f, NULL);
    CHECK(cterm_ui_has_focus(&f.ui, &f.a));
    CHECK(!cterm_ui_has_focus(&f.ui, &f.b_input));
    CHECK(!cterm_ui_has_focus(&f.ui, &f.list));
    cterm_buffer_free(f.b);
}

static void test_tab_order(void) {
    form f;
    form_init(&f);
    form_frame(&f, NULL);

    form_key(&f, CTERM_KEY_TAB, 0);
    CHECK(cterm_ui_has_focus(&f.ui, &f.b_input));
    CHECK(cterm_ui_event_handled(&f.ui));
    /* The new focus shows in the same frame: only one widget acts focused. */
    CHECK_INT(f.b->cursor_y, 1);

    form_key(&f, CTERM_KEY_TAB, 0);
    CHECK(cterm_ui_has_focus(&f.ui, &f.list));
    form_key(&f, CTERM_KEY_TAB, 0);
    CHECK(cterm_ui_has_focus(&f.ui, &f.a));   /* wraps around */

    form_key(&f, CTERM_KEY_TAB, CTERM_MOD_SHIFT);
    CHECK(cterm_ui_has_focus(&f.ui, &f.list)); /* wraps backwards */
    form_key(&f, CTERM_KEY_TAB, CTERM_MOD_SHIFT);
    CHECK(cterm_ui_has_focus(&f.ui, &f.b_input));
    cterm_buffer_free(f.b);
}

static void test_keys_only_reach_the_focused_widget(void) {
    form f;
    form_init(&f);
    form_frame(&f, NULL);

    form_key(&f, 'x', 0);
    CHECK_STR(f.text_a, "x");
    CHECK_STR(f.text_b, "");

    form_key(&f, CTERM_KEY_TAB, 0);
    form_key(&f, 'y', 0);
    CHECK_STR(f.text_a, "x");
    CHECK_STR(f.text_b, "y");

    /* The list only moves when it has the focus. */
    form_key(&f, CTERM_KEY_DOWN, 0);
    CHECK_INT(f.list.selected, 0);
    form_key(&f, CTERM_KEY_TAB, 0);
    form_key(&f, CTERM_KEY_DOWN, 0);
    CHECK_INT(f.list.selected, 1);
    cterm_buffer_free(f.b);
}

static void test_click_moves_focus(void) {
    form f;
    cterm_event ev;
    form_init(&f);
    form_frame(&f, NULL);

    ev = mouse(CTERM_MOUSE_PRESS, 3, 1);
    form_frame(&f, &ev);
    CHECK(cterm_ui_has_focus(&f.ui, &f.b_input));

    ev = mouse(CTERM_MOUSE_PRESS, 3, 3);
    form_frame(&f, &ev);
    CHECK(cterm_ui_has_focus(&f.ui, &f.list));
    CHECK_INT(f.list.selected, 1); /* the click selected a row too */
    cterm_buffer_free(f.b);
}

static void test_app_can_set_focus(void) {
    form f;
    form_init(&f);
    form_frame(&f, NULL);
    cterm_ui_focus(&f.ui, &f.list);
    form_frame(&f, NULL);
    CHECK(cterm_ui_has_focus(&f.ui, &f.list));
    cterm_buffer_free(f.b);
}

static void test_focused_widget_disappears(void) {
    form f;
    form_init(&f);
    form_frame(&f, NULL);
    form_key(&f, CTERM_KEY_TAB, 0);
    CHECK(cterm_ui_has_focus(&f.ui, &f.b_input));

    /* Tab order must still work after a widget goes away, and keys can't get lost. */
    f.show_b = 0;
    form_frame(&f, NULL);
    CHECK(cterm_ui_has_focus(&f.ui, &f.a));
    form_key(&f, 'k', 0);
    CHECK_STR(f.text_a, "k");
    form_key(&f, CTERM_KEY_TAB, 0);
    CHECK(cterm_ui_has_focus(&f.ui, &f.list));
    cterm_buffer_free(f.b);
}

static void test_no_widgets(void) {
    /* A frame with no widgets is fine, and Tab goes back to the app. */
    cterm_ui ui;
    cterm_buffer *b = cterm_buffer_new(5, 5);
    cterm_event ev = key_event(CTERM_KEY_TAB, 0);
    cterm_ui_init(&ui);
    cterm_ui_begin(&ui, b, &ev);
    cterm_ui_end(&ui);
    CHECK(!cterm_ui_event_handled(&ui));
    cterm_buffer_free(b);
}

static void test_more_widgets_than_the_limit(void) {
    /* Over the limit: everything still draws without overflowing the list. */
    enum { TOTAL = CTERM_UI_MAX_WIDGETS + 10 };
    static cterm_list lists[TOTAL];
    cterm_ui ui;
    cterm_buffer *b = cterm_buffer_new(5, 5);
    int i;
    cterm_ui_init(&ui);
    cterm_ui_begin(&ui, b, NULL);
    for (i = 0; i < TOTAL; i++) cterm_ui_list(&ui, cterm_rect_make(0, 0, 5, 1), &lists[i], fruit, 1);
    cterm_ui_end(&ui);
    CHECK_INT(ui.widget_count[ui.current], CTERM_UI_MAX_WIDGETS);
    cterm_buffer_free(b);
}

static void test_input_inside_scroll_view(void) {
    /* Widgets nest. Keys go to whichever has focus, and clicks to the inner one. */
    cterm_ui ui;
    cterm_scroll scroll;
    cterm_input input;
    char text[16];
    cterm_buffer *b = cterm_buffer_new(10, 3);
    cterm_event ev;
    int frame;

    cterm_ui_init(&ui);
    memset(&scroll, 0, sizeof scroll);
    cterm_input_init(&input, text, sizeof text);
    memset(&ev, 0, sizeof ev);

    for (frame = 0; frame < 5; frame++) {
        cterm_rect c;
        if (frame == 2) ev = mouse(CTERM_MOUSE_PRESS, 2, 1);
        else if (frame == 3) ev = key_event('q', 0);
        else if (frame == 4) ev = key_event(CTERM_KEY_DOWN, 0);
        cterm_clear(b);
        cterm_ui_begin(&ui, b, frame >= 2 ? &ev : NULL);
        c = cterm_ui_scroll_begin(&ui, cterm_bounds(b), &scroll, 0, 20);
        cterm_ui_input(&ui, cterm_rect_make(c.x, c.y + 1, 8, 1), &input);
        cterm_ui_scroll_end(&ui, &scroll);
        cterm_ui_end(&ui);

        if (frame == 2) CHECK(cterm_ui_has_focus(&ui, &input));
        if (frame == 3) CHECK_STR(text, "q");
    }
    /* Down isn't used by the input, and the scroll view doesn't have the focus. */
    CHECK_INT(scroll.y, 0);
    cterm_buffer_free(b);
}

static void test_input_scrolled_out_of_view(void) {
    /* A widget that's scrolled away can't be clicked, and shows no cursor. */
    cterm_ui ui;
    cterm_scroll scroll;
    cterm_input input;
    char text[16];
    cterm_buffer *b = cterm_buffer_new(10, 2);
    cterm_rect c;
    int frame;

    cterm_ui_init(&ui);
    memset(&scroll, 0, sizeof scroll);
    scroll.y = 5;
    cterm_input_init(&input, text, sizeof text);
    cterm_ui_focus(&ui, &input);
    for (frame = 0; frame < 2; frame++) {
        cterm_event ev = mouse(CTERM_MOUSE_PRESS, 1, 0);
        cterm_clear(b);
        cterm_ui_begin(&ui, b, frame == 1 ? &ev : NULL);
        c = cterm_ui_scroll_begin(&ui, cterm_bounds(b), &scroll, 0, 20);
        cterm_ui_input(&ui, cterm_rect_make(c.x, c.y, 8, 1), &input); /* at content row 0 */
        cterm_ui_scroll_end(&ui, &scroll);
        cterm_ui_end(&ui);
    }
    CHECK_INT(b->cursor_visible, 0);
    CHECK_INT(input.cursor, 0);
    cterm_buffer_free(b);
}

static void test_tiny_areas(void) {
    /* Very small terminals happen when people resize. Nothing may crash. */
    cterm_ui ui;
    cterm_input input;
    cterm_list list;
    cterm_scroll scroll;
    char text[16];
    int w, h;
    cterm_ui_init(&ui);
    cterm_input_init(&input, text, sizeof text);
    cterm_input_set(&input, "hello");
    memset(&list, 0, sizeof list);
    memset(&scroll, 0, sizeof scroll);
    for (w = 0; w < 4; w++) {
        for (h = 0; h < 4; h++) {
            cterm_buffer *b = cterm_buffer_new(w, h);
            cterm_rect area = cterm_bounds(b), c;
            cterm_event ev = key_event(CTERM_KEY_DOWN, 0);
            cterm_ui_begin(&ui, b, &ev);
            cterm_ui_input(&ui, area, &input);
            cterm_ui_list(&ui, area, &list, fruit, 10);
            c = cterm_ui_scroll_begin(&ui, area, &scroll, 0, 10);
            cterm_text(b, c.x, c.y, cterm_plain(), "x");
            cterm_ui_scroll_end(&ui, &scroll);
            cterm_ui_end(&ui);
            cterm_buffer_free(b);
        }
    }
    CHECK(1);
}

int main(void) {
    RUN(test_split_fixed_and_fill);
    RUN(test_split_columns);
    RUN(test_split_percent);
    RUN(test_split_weights_and_rounding);
    RUN(test_split_gap);
    RUN(test_split_not_enough_room);
    RUN(test_split_never_overflows);

    RUN(test_input_typing);
    RUN(test_input_editing);
    RUN(test_input_word_operations);
    RUN(test_input_unicode);
    RUN(test_input_capacity);
    RUN(test_input_paste);
    RUN(test_input_submit_and_unknown_keys);
    RUN(test_input_horizontal_scroll);
    RUN(test_input_mouse);
    RUN(test_input_placeholder_and_mask);
    RUN(test_input_survives_app_changes);
    RUN(test_input_stays_valid_under_fuzz);

    RUN(test_list_draws_rows);
    RUN(test_list_keys);
    RUN(test_list_scrolls_with_selection);
    RUN(test_list_mouse);
    RUN(test_list_empty_rows_and_clicks);
    RUN(test_list_count_shrinks);
    RUN(test_list_height_shrinks);
    RUN(test_list_scrollbar);
    RUN(test_list_callback_only_visible);
    RUN(test_list_long_text_is_clipped);

    RUN(test_scroll_draws_visible_part);
    RUN(test_scroll_keys);
    RUN(test_scroll_horizontal);
    RUN(test_scroll_mouse);
    RUN(test_scroll_content_fits);
    RUN(test_scroll_clips_and_restores);
    RUN(test_scroll_shrinking_content);

    RUN(test_focus_starts_on_first_widget);
    RUN(test_tab_order);
    RUN(test_keys_only_reach_the_focused_widget);
    RUN(test_click_moves_focus);
    RUN(test_app_can_set_focus);
    RUN(test_focused_widget_disappears);
    RUN(test_no_widgets);
    RUN(test_more_widgets_than_the_limit);
    RUN(test_input_inside_scroll_view);
    RUN(test_input_scrolled_out_of_view);
    RUN(test_tiny_areas);
    return test_summary();
}
