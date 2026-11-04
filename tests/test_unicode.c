/* Tests for UTF-8 decoding and encoding, and character widths. */
#define CTERM_IMPLEMENTATION
#include "cterm.h"
#include "test.h"

/* Decodes a string literal and checks both the result and the bytes consumed. */
static void check_decode(const char *bytes, size_t n, int used, uint32_t cp) {
    uint32_t out = 0xDEADBEEFu;
    int got = cterm__utf8_decode((const unsigned char *)bytes, n, &out);
    CHECK_INT(got, used);
    if (used > 0) CHECK_INT(out, cp);
}

static void test_decode_valid(void) {
    check_decode("A", 1, 1, 'A');
    check_decode("\xC3\xA9", 2, 2, 0xE9);         /* e with acute */
    check_decode("\xE0\xA6\x95", 3, 3, 0x995);    /* Bengali KA */
    check_decode("\xE4\xB8\xAD", 3, 3, 0x4E2D);   /* CJK */
    check_decode("\xF0\x9F\x98\x80", 4, 4, 0x1F600); /* emoji */
    check_decode("\xF4\x8F\xBF\xBF", 4, 4, 0x10FFFF); /* the last code point */
}

static void test_decode_incomplete(void) {
    /* A valid prefix returns 0, meaning "wait for more". */
    check_decode("", 0, 0, 0);
    check_decode("\xC3", 1, 0, 0);
    check_decode("\xE0\xA6", 2, 0, 0);
    check_decode("\xF0\x9F\x98", 3, 0, 0);
}

static void test_decode_invalid(void) {
    /* Invalid input gives U+FFFD and moves forward exactly one byte. */
    check_decode("\x80", 1, 1, CTERM__REPLACEMENT);       /* lone continuation */
    check_decode("\xFF", 1, 1, CTERM__REPLACEMENT);       /* never valid */
    check_decode("\xC0\x80", 2, 1, CTERM__REPLACEMENT);   /* overlong NUL */
    check_decode("\xC1\xBF", 2, 1, CTERM__REPLACEMENT);   /* overlong */
    check_decode("\xC3\x41", 2, 1, CTERM__REPLACEMENT);   /* bad continuation */
    check_decode("\xE0\x80\x80", 3, 1, CTERM__REPLACEMENT); /* overlong */
    check_decode("\xED\xA0\x80", 3, 1, CTERM__REPLACEMENT); /* surrogate */
    check_decode("\xF0\x80\x80\x80", 4, 1, CTERM__REPLACEMENT); /* overlong */
    check_decode("\xF4\x90\x80\x80", 4, 1, CTERM__REPLACEMENT); /* above U+10FFFF */
    check_decode("\xF5\x80\x80\x80", 4, 1, CTERM__REPLACEMENT); /* never valid */
}

static void test_encode_round_trip(void) {
    /* Every valid code point has to survive an encode and decode. */
    uint32_t cp;
    int failures = 0;
    for (cp = 0; cp <= 0x10FFFF; cp++) {
        char bytes[4];
        uint32_t back = 0;
        int n;
        if (cp >= 0xD800 && cp <= 0xDFFF) continue;
        n = cterm__utf8_encode(cp, bytes);
        if (cterm__utf8_decode((const unsigned char *)bytes, (size_t)n, &back) != n || back != cp) failures++;
    }
    CHECK_INT(failures, 0);
}

static void test_char_width(void) {
    CHECK_INT(cterm_char_width('a'), 1);
    CHECK_INT(cterm_char_width(0xE9), 1);     /* e with acute */
    CHECK_INT(cterm_char_width(0x2500), 1);   /* box drawing */

    CHECK_INT(cterm_char_width(0x0301), 0);   /* combining acute accent */
    CHECK_INT(cterm_char_width(0x200D), 0);   /* zero width joiner */
    CHECK_INT(cterm_char_width(0xFE0F), 0);   /* variation selector 16 */
    CHECK_INT(cterm_char_width(0x09CD), 0);   /* Bengali virama */
    CHECK_INT(cterm_char_width(0x09C1), 0);   /* Bengali vowel sign U */
    CHECK_INT(cterm_char_width(0x1160), 0);   /* Hangul Jamo vowel */

    CHECK_INT(cterm_char_width(0x0995), 1);   /* Bengali KA */
    CHECK_INT(cterm_char_width(0x09BE), 1);   /* Bengali vowel sign AA: spacing */
    CHECK_INT(cterm_char_width(0x00AD), 1);   /* soft hyphen: visible */
    CHECK_INT(cterm_char_width(0x0600), 1);   /* Arabic number sign: visible */

    CHECK_INT(cterm_char_width(0x4E2D), 2);   /* CJK */
    CHECK_INT(cterm_char_width(0xAC00), 2);   /* Hangul syllable */
    CHECK_INT(cterm_char_width(0xFF21), 2);   /* fullwidth A */
    CHECK_INT(cterm_char_width(0x1F600), 2);  /* emoji */

    CHECK_INT(cterm_char_width(0x2764), 1);   /* heart: a text symbol by default */
}

static void test_text_width(void) {
    CHECK_INT(cterm_text_width(""), 0);
    CHECK_INT(cterm_text_width("hello"), 5);
    CHECK_INT(cterm_text_width("e\xCC\x81"), 1);          /* e + combining accent */
    CHECK_INT(cterm_text_width("\xE4\xB8\xAD\xE6\x96\x87"), 4); /* two CJK */
    CHECK_INT(cterm_text_width("\xFF"), 1);               /* invalid byte */
    CHECK_INT(cterm_text_width("a\xE0\xA6"), 3);          /* truncated at the end */
}

int main(void) {
    RUN(test_decode_valid);
    RUN(test_decode_incomplete);
    RUN(test_decode_invalid);
    RUN(test_encode_round_trip);
    RUN(test_char_width);
    RUN(test_text_width);
    return test_summary();
}
