/* Tests for the input parser. No terminal needed: bytes go straight in. */
#define CTERM_IMPLEMENTATION
#include "cterm.h"
#include "test.h"

/* Parses one complete input and checks it gives a single key event. It's run three
 * ways: all at once, split at every position, and one byte at a time. Terminals
 * can deliver bytes in pieces, and the result must always be the same. */
static void check_key_at(const char *bytes, size_t n, int key, int mods, int line) {
    size_t split;
    for (split = 0; split <= n; split++) {
        cterm__parser p;
        cterm_event ev;
        int got;
        memset(&p, 0, sizeof p);
        cterm__parser_feed(&p, (const unsigned char *)bytes, split);
        got = cterm__parser_next(&p, &ev, 0);
        if (split < n && got) {
            printf("  note: line %d returned early at split %d\n", line, (int)split);
        }
        if (!got) {
            cterm__parser_feed(&p, (const unsigned char *)bytes + split, n - split);
            got = cterm__parser_next(&p, &ev, 0);
            if (!got) got = cterm__parser_next(&p, &ev, 1); /* a lone Esc needs the timeout */
        }
        test_checks++;
        if (got != 1 || ev.type != CTERM_EVENT_KEY || ev.key != key || ev.mods != mods) {
            test_failures++;
            printf("  FAIL line %d split %d: got=%d type=%d key=0x%X mods=%d, expected key=0x%X mods=%d\n",
                   line, (int)split, got, (int)ev.type, (unsigned)ev.key, ev.mods, (unsigned)key, mods);
        }
        /* Nothing should be left over. */
        CHECK_INT(cterm__parser_next(&p, &ev, 1), 0);
        cterm__parser_free(&p);
    }
}

#define KEY(bytes, key, mods) check_key_at(bytes, sizeof(bytes) - 1, key, mods, __LINE__)

static void test_plain_keys(void) {
    KEY("a", 'a', 0);
    KEY("A", 'A', 0);
    KEY(" ", ' ', 0);
    KEY("\xC3\xA9", 0xE9, 0);
    KEY("\xE0\xA6\x95", 0x995, 0);
    KEY("\xF0\x9F\x98\x80", 0x1F600, 0);
}

static void test_control_keys(void) {
    KEY("\r", CTERM_KEY_ENTER, 0);
    KEY("\t", CTERM_KEY_TAB, 0);
    KEY("\x7f", CTERM_KEY_BACKSPACE, 0);
    KEY("\x08", CTERM_KEY_BACKSPACE, 0);
    KEY("\x01", 'a', CTERM_MOD_CTRL);
    KEY("\x03", 'c', CTERM_MOD_CTRL);
    KEY("\x1a", 'z', CTERM_MOD_CTRL);
    KEY("\n", 'j', CTERM_MOD_CTRL);
    KEY("\x1c", '\\', CTERM_MOD_CTRL);
    KEY("\x1d", ']', CTERM_MOD_CTRL);
    KEY("\x1e", '^', CTERM_MOD_CTRL);
    KEY("\x1f", '_', CTERM_MOD_CTRL);
    {
        /* Ctrl+Space is a NUL byte, which can't go through a string literal test. */
        check_key_at("\0", 1, ' ', CTERM_MOD_CTRL, __LINE__);
    }
}

static void test_arrows_and_navigation(void) {
    KEY("\x1b[A", CTERM_KEY_UP, 0);
    KEY("\x1b[B", CTERM_KEY_DOWN, 0);
    KEY("\x1b[C", CTERM_KEY_RIGHT, 0);
    KEY("\x1b[D", CTERM_KEY_LEFT, 0);
    KEY("\x1b[H", CTERM_KEY_HOME, 0);
    KEY("\x1b[F", CTERM_KEY_END, 0);
    KEY("\x1bOA", CTERM_KEY_UP, 0);   /* application mode */
    KEY("\x1bOD", CTERM_KEY_LEFT, 0);
    KEY("\x1bOH", CTERM_KEY_HOME, 0);
    KEY("\x1b[1~", CTERM_KEY_HOME, 0);
    KEY("\x1b[7~", CTERM_KEY_HOME, 0);
    KEY("\x1b[4~", CTERM_KEY_END, 0);
    KEY("\x1b[8~", CTERM_KEY_END, 0);
    KEY("\x1b[2~", CTERM_KEY_INSERT, 0);
    KEY("\x1b[3~", CTERM_KEY_DELETE, 0);
    KEY("\x1b[5~", CTERM_KEY_PAGE_UP, 0);
    KEY("\x1b[6~", CTERM_KEY_PAGE_DOWN, 0);
    KEY("\x1b[Z", CTERM_KEY_TAB, CTERM_MOD_SHIFT);
    KEY("\x1bOM", CTERM_KEY_ENTER, 0); /* keypad Enter */
}

static void test_function_keys(void) {
    KEY("\x1bOP", CTERM_KEY_F1, 0);
    KEY("\x1bOQ", CTERM_KEY_F2, 0);
    KEY("\x1bOR", CTERM_KEY_F3, 0);
    KEY("\x1bOS", CTERM_KEY_F4, 0);
    KEY("\x1b[11~", CTERM_KEY_F1, 0);
    KEY("\x1b[15~", CTERM_KEY_F5, 0);
    KEY("\x1b[17~", CTERM_KEY_F6, 0);
    KEY("\x1b[18~", CTERM_KEY_F7, 0);
    KEY("\x1b[19~", CTERM_KEY_F8, 0);
    KEY("\x1b[20~", CTERM_KEY_F9, 0);
    KEY("\x1b[21~", CTERM_KEY_F10, 0);
    KEY("\x1b[23~", CTERM_KEY_F11, 0);
    KEY("\x1b[24~", CTERM_KEY_F12, 0);
    KEY("\x1b[[A", CTERM_KEY_F1, 0);   /* Linux console */
    KEY("\x1b[[E", CTERM_KEY_F5, 0);
}

static void test_modifiers(void) {
    KEY("\x1b[1;2A", CTERM_KEY_UP, CTERM_MOD_SHIFT);
    KEY("\x1b[1;3A", CTERM_KEY_UP, CTERM_MOD_ALT);
    KEY("\x1b[1;5A", CTERM_KEY_UP, CTERM_MOD_CTRL);
    KEY("\x1b[1;6C", CTERM_KEY_RIGHT, CTERM_MOD_CTRL | CTERM_MOD_SHIFT);
    KEY("\x1b[1;8D", CTERM_KEY_LEFT, CTERM_MOD_CTRL | CTERM_MOD_ALT | CTERM_MOD_SHIFT);
    KEY("\x1b[3;5~", CTERM_KEY_DELETE, CTERM_MOD_CTRL);
    KEY("\x1b[15;2~", CTERM_KEY_F5, CTERM_MOD_SHIFT);
    KEY("\x1b[1;2P", CTERM_KEY_F1, CTERM_MOD_SHIFT);
    KEY("\x1bO5A", CTERM_KEY_UP, CTERM_MOD_CTRL);
    KEY("\x1b\x1b[A", CTERM_KEY_UP, CTERM_MOD_ALT);

    /* Alt + a key is ESC + that key. */
    KEY("\x1b" "a", 'a', CTERM_MOD_ALT);
    KEY("\x1b\r", CTERM_KEY_ENTER, CTERM_MOD_ALT);
    KEY("\x1b\x7f", CTERM_KEY_BACKSPACE, CTERM_MOD_ALT);
    KEY("\x1b\x03", 'c', CTERM_MOD_CTRL | CTERM_MOD_ALT);
    KEY("\x1b\xC3\xA9", 0xE9, CTERM_MOD_ALT);
}

static void test_newer_key_protocols(void) {
    /* "CSI u" and modifyOtherKeys can tell apart keys the old protocol couldn't. */
    KEY("\x1b[13;5u", CTERM_KEY_ENTER, CTERM_MOD_CTRL);
    KEY("\x1b[9;2u", CTERM_KEY_TAB, CTERM_MOD_SHIFT);
    KEY("\x1b[27u", CTERM_KEY_ESC, 0);
    KEY("\x1b[127;5u", CTERM_KEY_BACKSPACE, CTERM_MOD_CTRL);
    KEY("\x1b[97;5u", 'a', CTERM_MOD_CTRL);
    KEY("\x1b[97:65;5u", 'a', CTERM_MOD_CTRL); /* with kitty's extra detail */
    KEY("\x1b[27;5;13~", CTERM_KEY_ENTER, CTERM_MOD_CTRL);
}

static void test_esc_needs_flush(void) {
    cterm__parser p;
    cterm_event ev;
    memset(&p, 0, sizeof p);

    /* A lone ESC could be the start of a sequence, so it has to wait... */
    cterm__parser_feed(&p, (const unsigned char *)"\x1b", 1);
    CHECK_INT(cterm__parser_next(&p, &ev, 0), 0);
    CHECK(cterm__parser_pending(&p));

    /* ...until the timeout says nothing else is coming. */
    CHECK_INT(cterm__parser_next(&p, &ev, 1), 1);
    CHECK_INT(ev.key, CTERM_KEY_ESC);
    CHECK_INT(ev.mods, 0);
    CHECK(!cterm__parser_pending(&p));

    /* Esc pressed twice is two presses, not Alt+Esc. */
    cterm__parser_feed(&p, (const unsigned char *)"\x1b\x1b", 2);
    CHECK_INT(cterm__parser_next(&p, &ev, 0), 0);
    CHECK_INT(cterm__parser_next(&p, &ev, 1), 1);
    CHECK_INT(ev.key, CTERM_KEY_ESC);
    CHECK_INT(cterm__parser_next(&p, &ev, 1), 1);
    CHECK_INT(ev.key, CTERM_KEY_ESC);
    CHECK_INT(ev.mods, 0);

    /* ESC [ with nothing after it was really Alt+[ */
    cterm__parser_feed(&p, (const unsigned char *)"\x1b[", 2);
    CHECK_INT(cterm__parser_next(&p, &ev, 0), 0);
    CHECK_INT(cterm__parser_next(&p, &ev, 1), 1);
    CHECK_INT(ev.key, '[');
    CHECK_INT(ev.mods, CTERM_MOD_ALT);

    cterm__parser_feed(&p, (const unsigned char *)"\x1bO", 2);
    CHECK_INT(cterm__parser_next(&p, &ev, 1), 1);
    CHECK_INT(ev.key, 'O');
    CHECK_INT(ev.mods, CTERM_MOD_ALT);

    /* A half-finished sequence at the timeout is dropped, not typed as text. */
    cterm__parser_feed(&p, (const unsigned char *)"\x1b[1;5", 5);
    CHECK_INT(cterm__parser_next(&p, &ev, 0), 0);
    CHECK_INT(cterm__parser_next(&p, &ev, 1), 0);
    CHECK(!cterm__parser_pending(&p));

    /* A truncated UTF-8 character becomes U+FFFD. */
    cterm__parser_feed(&p, (const unsigned char *)"\xE0\xA6", 2);
    CHECK_INT(cterm__parser_next(&p, &ev, 0), 0);
    CHECK_INT(cterm__parser_next(&p, &ev, 1), 1);
    CHECK_INT(ev.key, CTERM__REPLACEMENT);
    cterm__parser_free(&p);
}

/* Feeds bytes and returns the first event, which has to exist. */
static cterm_event mouse_event(const char *bytes) {
    cterm__parser p;
    cterm_event ev;
    memset(&p, 0, sizeof p);
    cterm__parser_feed(&p, (const unsigned char *)bytes, strlen(bytes));
    CHECK_INT(cterm__parser_next(&p, &ev, 0), 1);
    CHECK_INT(ev.type, CTERM_EVENT_MOUSE);
    cterm__parser_free(&p);
    return ev;
}

static void test_mouse_sgr(void) {
    cterm_event ev = mouse_event("\x1b[<0;10;5M");
    CHECK_INT(ev.action, CTERM_MOUSE_PRESS);
    CHECK_INT(ev.button, CTERM_BUTTON_LEFT);
    CHECK_INT(ev.x, 9);  /* cterm counts from 0 */
    CHECK_INT(ev.y, 4);

    ev = mouse_event("\x1b[<0;10;5m");
    CHECK_INT(ev.action, CTERM_MOUSE_RELEASE);
    CHECK_INT(ev.button, CTERM_BUTTON_LEFT);

    ev = mouse_event("\x1b[<1;1;1M");
    CHECK_INT(ev.button, CTERM_BUTTON_MIDDLE);
    ev = mouse_event("\x1b[<2;1;1M");
    CHECK_INT(ev.button, CTERM_BUTTON_RIGHT);

    ev = mouse_event("\x1b[<32;7;8M");
    CHECK_INT(ev.action, CTERM_MOUSE_DRAG);
    CHECK_INT(ev.button, CTERM_BUTTON_LEFT);
    CHECK_INT(ev.x, 6);

    ev = mouse_event("\x1b[<35;7;8M");
    CHECK_INT(ev.action, CTERM_MOUSE_MOVE);
    CHECK_INT(ev.button, CTERM_BUTTON_NONE);

    ev = mouse_event("\x1b[<64;3;4M");
    CHECK_INT(ev.action, CTERM_MOUSE_SCROLL_UP);
    ev = mouse_event("\x1b[<65;3;4M");
    CHECK_INT(ev.action, CTERM_MOUSE_SCROLL_DOWN);
    ev = mouse_event("\x1b[<66;3;4M");
    CHECK_INT(ev.action, CTERM_MOUSE_SCROLL_LEFT);
    ev = mouse_event("\x1b[<67;3;4M");
    CHECK_INT(ev.action, CTERM_MOUSE_SCROLL_RIGHT);

    ev = mouse_event("\x1b[<4;1;1M");
    CHECK_INT(ev.mods, CTERM_MOD_SHIFT);
    ev = mouse_event("\x1b[<8;1;1M");
    CHECK_INT(ev.mods, CTERM_MOD_ALT);
    ev = mouse_event("\x1b[<16;1;1M");
    CHECK_INT(ev.mods, CTERM_MOD_CTRL);

    /* Past column 223, which the old mouse format couldn't express. */
    ev = mouse_event("\x1b[<0;500;300M");
    CHECK_INT(ev.x, 499);
    CHECK_INT(ev.y, 299);

    /* Coordinates of 0 don't occur, but they must not go negative. */
    ev = mouse_event("\x1b[<0;0;0M");
    CHECK_INT(ev.x, 0);
    CHECK_INT(ev.y, 0);
}

static void test_mouse_legacy(void) {
    /* ESC [ M, then button + 32, x + 33, y + 33. */
    cterm_event ev = mouse_event("\x1b[M\x20\x2A\x25");
    CHECK_INT(ev.action, CTERM_MOUSE_PRESS);
    CHECK_INT(ev.button, CTERM_BUTTON_LEFT);
    CHECK_INT(ev.x, 9);
    CHECK_INT(ev.y, 4);

    ev = mouse_event("\x1b[M\x23\x2A\x25");
    CHECK_INT(ev.action, CTERM_MOUSE_RELEASE);

    ev = mouse_event("\x1b[M\x60\x21\x21");
    CHECK_INT(ev.action, CTERM_MOUSE_SCROLL_UP);
}

static void test_paste(void) {
    cterm__parser p;
    cterm_event ev;
    memset(&p, 0, sizeof p);

    cterm__parser_feed(&p, (const unsigned char *)"\x1b[200~hello\x1b[201~x", 18);
    CHECK_INT(cterm__parser_next(&p, &ev, 0), 1);
    CHECK_INT(ev.type, CTERM_EVENT_PASTE);
    CHECK_STR(ev.text, "hello");
    CHECK_INT(ev.text_length, 5);
    /* Typing continues normally afterwards. */
    CHECK_INT(cterm__parser_next(&p, &ev, 0), 1);
    CHECK_INT(ev.key, 'x');

    /* Keys inside a paste are text: an arrow sequence here must not act as a key. */
    cterm__parser_feed(&p, (const unsigned char *)"\x1b[200~a\x1b[Ab\r\x1b[201~", 18);
    CHECK_INT(cterm__parser_next(&p, &ev, 0), 1);
    CHECK_INT(ev.type, CTERM_EVENT_PASTE);
    CHECK_STR(ev.text, "a\x1b[Ab\r");

    /* An empty paste. */
    cterm__parser_feed(&p, (const unsigned char *)"\x1b[200~\x1b[201~", 12);
    CHECK_INT(cterm__parser_next(&p, &ev, 0), 1);
    CHECK_INT(ev.type, CTERM_EVENT_PASTE);
    CHECK_STR(ev.text, "");
    CHECK_INT(ev.text_length, 0);
    cterm__parser_free(&p);
}

static void test_paste_in_pieces(void) {
    /* The end marker can be split anywhere, so feed one byte at a time. */
    const char *input = "\x1b[200~line one\nline two\x1b[201~";
    size_t n = strlen(input), i;
    cterm__parser p;
    cterm_event ev;
    int got = 0;
    memset(&p, 0, sizeof p);
    for (i = 0; i < n; i++) {
        cterm__parser_feed(&p, (const unsigned char *)input + i, 1);
        got += cterm__parser_next(&p, &ev, 0);
        if (got) break;
    }
    CHECK_INT(got, 1);
    CHECK_INT(i, n - 1); /* only complete on the very last byte */
    CHECK_STR(ev.text, "line one\nline two");

    /* Text that nearly matches the end marker has to survive. */
    {
        const char *tricky = "\x1b[200~\x1b[201x and \x1b[20\x1b[201~";
        cterm__parser_feed(&p, (const unsigned char *)tricky, strlen(tricky));
        CHECK_INT(cterm__parser_next(&p, &ev, 0), 1);
        CHECK_STR(ev.text, "\x1b[201x and \x1b[20");
    }
    cterm__parser_free(&p);
}

static void test_paste_without_end(void) {
    /* A broken terminal, or a lost connection, must not swallow input forever. */
    cterm__parser p;
    cterm_event ev;
    memset(&p, 0, sizeof p);
    cterm__parser_feed(&p, (const unsigned char *)"\x1b[200~partial", 13);
    CHECK_INT(cterm__parser_next(&p, &ev, 0), 0);
    CHECK(cterm__parser_pending(&p));
    CHECK_INT(cterm__parser_next(&p, &ev, 1), 1);
    CHECK_INT(ev.type, CTERM_EVENT_PASTE);
    CHECK_STR(ev.text, "partial");
    CHECK(!cterm__parser_pending(&p));
    cterm__parser_free(&p);
}

static void test_big_paste(void) {
    /* Larger than any internal buffer, delivered in chunks. */
    enum { CHUNK = 4096, CHUNKS = 300 };
    static char chunk[CHUNK];
    cterm__parser p;
    cterm_event ev;
    int i, got = 0;
    memset(&p, 0, sizeof p);
    memset(chunk, 'x', sizeof chunk);
    cterm__parser_feed(&p, (const unsigned char *)"\x1b[200~", 6);
    for (i = 0; i < CHUNKS; i++) {
        cterm__parser_feed(&p, (const unsigned char *)chunk, sizeof chunk);
        got += cterm__parser_next(&p, &ev, 0);
    }
    cterm__parser_feed(&p, (const unsigned char *)"\x1b[201~", 6);
    got += cterm__parser_next(&p, &ev, 0);
    CHECK_INT(got, 1);
    CHECK_INT(ev.text_length, CHUNK * CHUNKS);
    CHECK_INT(strlen(ev.text), CHUNK * CHUNKS);
    cterm__parser_free(&p);
}

/* Counts the events that come out of some input. */
static int count_events(const char *bytes, size_t n, int flush) {
    cterm__parser p;
    cterm_event ev;
    int count = 0;
    memset(&p, 0, sizeof p);
    cterm__parser_feed(&p, (const unsigned char *)bytes, n);
    while (cterm__parser_next(&p, &ev, flush)) count++;
    cterm__parser_free(&p);
    return count;
}

static void test_unknown_sequences_are_ignored(void) {
    /* Unknown sequences must vanish, not get typed into the app as text. */
    CHECK_INT(count_events("\x1b[999~", 6, 0), 0);
    CHECK_INT(count_events("\x1b[I", 3, 0), 0);        /* focus in */
    CHECK_INT(count_events("\x1b[O", 3, 0), 0);        /* focus out */
    CHECK_INT(count_events("\x1b[?62;c", 7, 0), 0);    /* device attributes reply */
    CHECK_INT(count_events("\x1b[>0;1;0c", 9, 0), 0);
    CHECK_INT(count_events("\x1b[1 q", 5, 0), 0);      /* intermediate byte */
    CHECK_INT(count_events("\x1bOz", 3, 0), 0);
    CHECK_INT(count_events("\x1b[201~", 6, 0), 0);     /* stray paste end */

    /* What follows is still read correctly. */
    CHECK_INT(count_events("\x1b[999~a", 7, 0), 1);
}

static void test_broken_sequences_recover(void) {
    cterm__parser p;
    cterm_event ev;
    memset(&p, 0, sizeof p);

    /* A sequence cut off by a new one: the new one still works. */
    cterm__parser_feed(&p, (const unsigned char *)"\x1b[1;\x1b[A", 7);
    CHECK_INT(cterm__parser_next(&p, &ev, 0), 1);
    CHECK_INT(ev.key, CTERM_KEY_UP);
    CHECK_INT(cterm__parser_next(&p, &ev, 1), 0);

    /* A very long run of parameters can't grow forever. */
    {
        static char junk[CTERM__CSI_MAX * 3];
        size_t i;
        junk[0] = 0x1b;
        junk[1] = '[';
        for (i = 2; i < sizeof junk; i++) junk[i] = '1';
        cterm__parser_feed(&p, (const unsigned char *)junk, sizeof junk);
        while (cterm__parser_next(&p, &ev, 1)) {}
        CHECK(!cterm__parser_pending(&p));
    }

    /* Too many parameters must not overflow. */
    CHECK_INT(count_events("\x1b[1;2;3;4;5;6;7;8;9;10;11;12;13A", 32, 0), 1);
    cterm__parser_free(&p);
}

/* Throws random bytes at the parser. It must never crash (the sanitizer build checks
 * this properly), never get stuck, and always empty out once flushed. */
static void test_random_bytes(void) {
    uint32_t state = 99;
    int run;
    for (run = 0; run < 3000; run++) {
        cterm__parser p;
        cterm_event ev;
        int i, len, guard = 0;
        memset(&p, 0, sizeof p);
        len = (int)((state = state * 1664525u + 1013904223u) >> 24) % 80;
        for (i = 0; i < len; i++) {
            static const unsigned char common[] = { 0x1b, '[', 'O', '<', ';', '~', '2', '0', 'M', 'm', 0xE0, 0xF0, 0x80 };
            unsigned char byte;
            state = state * 1664525u + 1013904223u;
            byte = (state >> 28) < 10 ? common[(state >> 16) % sizeof common] : (unsigned char)(state >> 16);
            cterm__parser_feed(&p, &byte, 1);
            if ((state >> 8) & 1) while (cterm__parser_next(&p, &ev, 0)) {}
        }
        while (cterm__parser_next(&p, &ev, 1) && guard < 1000) guard++;
        /* One more flush handles a paste that ended the previous loop. */
        while (cterm__parser_next(&p, &ev, 1) && guard < 1000) guard++;
        CHECK(guard < 1000);
        CHECK(!cterm__parser_pending(&p));
        cterm__parser_free(&p);
    }
}

int main(void) {
    RUN(test_plain_keys);
    RUN(test_control_keys);
    RUN(test_arrows_and_navigation);
    RUN(test_function_keys);
    RUN(test_modifiers);
    RUN(test_newer_key_protocols);
    RUN(test_esc_needs_flush);
    RUN(test_mouse_sgr);
    RUN(test_mouse_legacy);
    RUN(test_paste);
    RUN(test_paste_in_pieces);
    RUN(test_paste_without_end);
    RUN(test_big_paste);
    RUN(test_unknown_sequences_are_ignored);
    RUN(test_broken_sequences_recover);
    RUN(test_random_bytes);
    return test_summary();
}
