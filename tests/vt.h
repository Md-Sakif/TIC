/* vt.h - a miniature terminal emulator for tests.
 *
 * It reads the bytes the renderer produces and applies them to its own grid, the
 * way a real terminal would. A test can then ask: "after these bytes, does the
 * screen look exactly like the buffer the app drew?" That's much stronger than
 * comparing the bytes themselves, because it doesn't care how the renderer got
 * there, only that the result is correct.
 *
 * It copies real terminal behaviour for the things that matter to the renderer.
 * Writing over half of a wide character blanks the other half, and with auto-wrap
 * off, text at the right edge stays there.
 *
 * It aborts the test on any escape sequence it doesn't know, so the renderer
 * can't start sending something new without a test noticing.
 */
#ifndef CTERM_TEST_VT_H
#define CTERM_TEST_VT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct vt {
    int width, height;
    cterm_cell *cells;
    int x, y;
    cterm_style style;
    int cursor_visible;
    int sync_depth_errors; /* a synchronized update was started twice or never ended */
    int in_sync;
} vt;

static cterm_cell *vt_at(vt *t, int x, int y) {
    return &t->cells[y * t->width + x];
}

static void vt_erase(vt *t) {
    int i;
    for (i = 0; i < t->width * t->height; i++) {
        memset(&t->cells[i], 0, sizeof(cterm_cell));
        t->cells[i].ch = ' ';
    }
}

static void vt_init(vt *t, int width, int height) {
    memset(t, 0, sizeof *t);
    t->width = width;
    t->height = height;
    t->cells = (cterm_cell *)malloc(sizeof(cterm_cell) * (size_t)(width * height + 1));
    t->style = cterm_plain();
    vt_erase(t);
}

static void vt_free(vt *t) {
    free(t->cells);
}

/* Reads one SGR colour starting at params[i]. Returns how many params it used. */
static int vt_color(const int *params, int i, int count, cterm_color *out) {
    if (i + 2 < count && params[i + 1] == 5) {
        *out = CTERM_COLOR256(params[i + 2]);
        return 3;
    }
    if (i + 4 < count && params[i + 1] == 2) {
        *out = CTERM_RGB(params[i + 2], params[i + 3], params[i + 4]);
        return 5;
    }
    printf("vt: malformed colour sequence\n");
    abort();
}

static void vt_sgr(vt *t, const int *params, int count) {
    int i = 0;
    while (i < count) {
        int p = params[i];
        if (p == 0) t->style = cterm_plain();
        else if (p == 1) t->style.attrs |= CTERM_BOLD;
        else if (p == 2) t->style.attrs |= CTERM_DIM;
        else if (p == 3) t->style.attrs |= CTERM_ITALIC;
        else if (p == 4) t->style.attrs |= CTERM_UNDERLINE;
        else if (p == 5) t->style.attrs |= CTERM_BLINK;
        else if (p == 7) t->style.attrs |= CTERM_REVERSE;
        else if (p == 9) t->style.attrs |= CTERM_STRIKE;
        else if (p >= 30 && p <= 37) t->style.fg = CTERM_COLOR256(p - 30);
        else if (p >= 40 && p <= 47) t->style.bg = CTERM_COLOR256(p - 40);
        else if (p >= 90 && p <= 97) t->style.fg = CTERM_COLOR256(p - 90 + 8);
        else if (p >= 100 && p <= 107) t->style.bg = CTERM_COLOR256(p - 100 + 8);
        else if (p == 38) { i += vt_color(params, i, count, &t->style.fg); continue; }
        else if (p == 48) { i += vt_color(params, i, count, &t->style.bg); continue; }
        else {
            printf("vt: unknown SGR parameter %d\n", p);
            abort();
        }
        i++;
    }
}

/* Blanks the other half of any wide character that (x, y) belongs to. */
static void vt_split(vt *t, int x, int y) {
    if (vt_at(t, x, y)->ch == 0 && x > 0) {
        cterm_style keep = vt_at(t, x - 1, y)->style;
        memset(vt_at(t, x - 1, y), 0, sizeof(cterm_cell));
        vt_at(t, x - 1, y)->ch = ' ';
        vt_at(t, x - 1, y)->style = keep;
    }
    if (x + 1 < t->width && vt_at(t, x + 1, y)->ch == 0) {
        cterm_style keep = vt_at(t, x + 1, y)->style;
        memset(vt_at(t, x + 1, y), 0, sizeof(cterm_cell));
        vt_at(t, x + 1, y)->ch = ' ';
        vt_at(t, x + 1, y)->style = keep;
    }
}

static void vt_print(vt *t, uint32_t cp) {
    int width = cterm_char_width(cp);
    if (width == 0) {
        /* A combining mark joins the character just printed. */
        int x = t->x - 1, i;
        cterm_cell *cell;
        if (x < 0) return;
        cell = vt_at(t, x, t->y);
        if (cell->ch == 0 && x > 0) cell = vt_at(t, x - 1, t->y);
        for (i = 0; i < CTERM_MAX_MARKS; i++) {
            if (cell->marks[i] == 0) {
                cell->marks[i] = cp;
                break;
            }
        }
        return;
    }
    if (t->x + width > t->width) { /* auto-wrap off: stick to the right edge */
        t->x = t->width - width;
        if (t->x < 0) return;
    }
    vt_split(t, t->x, t->y);
    if (width == 2) vt_split(t, t->x + 1, t->y);
    memset(vt_at(t, t->x, t->y), 0, sizeof(cterm_cell));
    vt_at(t, t->x, t->y)->ch = cp;
    vt_at(t, t->x, t->y)->style = t->style;
    if (width == 2) {
        memset(vt_at(t, t->x + 1, t->y), 0, sizeof(cterm_cell));
        vt_at(t, t->x + 1, t->y)->style = t->style;
    }
    t->x += width;
}

/* Handles a CSI sequence: ESC [ params final. Returns the bytes consumed. */
static size_t vt_csi(vt *t, const char *s, size_t n) {
    size_t i = 2;
    int params[32], count = 0, value = 0, have = 0, is_private = 0;
    char final;

    if (i < n && s[i] == '?') {
        is_private = 1;
        i++;
    }
    while (i < n && ((s[i] >= '0' && s[i] <= '9') || s[i] == ';')) {
        if (s[i] == ';') {
            params[count++] = value;
            value = 0;
            have = 0;
        } else {
            value = value * 10 + (s[i] - '0');
            have = 1;
        }
        i++;
    }
    if (have || count > 0) params[count++] = value;
    if (i >= n) {
        printf("vt: truncated escape sequence\n");
        abort();
    }
    final = s[i++];

    if (is_private && (final == 'h' || final == 'l')) {
        int on = final == 'h';
        if (count == 1 && params[0] == 25) t->cursor_visible = on;
        else if (count == 1 && params[0] == 2026) {
            if (on == t->in_sync) t->sync_depth_errors++;
            t->in_sync = on;
        } else {
            printf("vt: unknown private mode %d\n", count ? params[0] : -1);
            abort();
        }
    } else if (final == 'H') {
        t->y = (count > 0 && params[0] > 0 ? params[0] : 1) - 1;
        t->x = (count > 1 && params[1] > 0 ? params[1] : 1) - 1;
        if (t->x >= t->width || t->y >= t->height) {
            printf("vt: cursor moved off screen to %d,%d\n", t->x, t->y);
            abort();
        }
    } else if (final == 'J' && count == 1 && params[0] == 2) {
        int i2;
        for (i2 = 0; i2 < t->width * t->height; i2++) {
            memset(&t->cells[i2], 0, sizeof(cterm_cell));
            t->cells[i2].ch = ' ';
            t->cells[i2].style = t->style;
        }
    } else if (final == 'm') {
        if (count == 0) t->style = cterm_plain();
        else vt_sgr(t, params, count);
    } else {
        printf("vt: unknown sequence ending in '%c'\n", final);
        abort();
    }
    return i;
}

/* Applies bytes to the screen. */
static void vt_feed(vt *t, const char *s, size_t n) {
    size_t i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        if (c == 0x1b) {
            if (i + 1 >= n || s[i + 1] != '[') {
                printf("vt: unknown escape sequence\n");
                abort();
            }
            i += vt_csi(t, s + i, n - i);
        } else if (c < 0x20 || c == 0x7f) {
            /* The renderer positions everything explicitly, and control codes
             * reaching the terminal would be a security problem. */
            printf("vt: control code 0x%02x sent to the terminal\n", c);
            abort();
        } else {
            uint32_t cp = 0;
            int used = cterm__utf8_decode((const unsigned char *)s + i, n - i, &cp);
            if (used <= 0) {
                printf("vt: truncated UTF-8\n");
                abort();
            }
            vt_print(t, cp);
            i += (size_t)used;
        }
    }
}

/* Returns the number of cells that differ from `b`, and prints the first one. */
static int vt_compare(vt *t, const cterm_buffer *b) {
    int x, y, m, differences = 0;
    for (y = 0; y < b->height; y++) {
        for (x = 0; x < b->width; x++) {
            const cterm_cell *want = &b->cells[y * b->width + x];
            const cterm_cell *have = vt_at(t, x, y);
            int same = want->ch == have->ch && want->style.fg == have->style.fg &&
                       want->style.bg == have->style.bg && want->style.attrs == have->style.attrs;
            for (m = 0; m < CTERM_MAX_MARKS; m++) {
                if (want->marks[m] != have->marks[m]) same = 0;
            }
            if (!same) {
                if (differences == 0) {
                    printf("  vt: first difference at (%d,%d): want U+%04X, have U+%04X\n", x, y,
                           (unsigned)want->ch, (unsigned)have->ch);
                }
                differences++;
            }
        }
    }
    return differences;
}

#endif /* CTERM_TEST_VT_H */
