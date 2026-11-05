/* widgets.c - a small file-picker-style app showing every widget and the layout
 * system working together:
 *
 *   - type in the search box to filter the list
 *   - the list shows the matches, and Enter picks one
 *   - the scrollable view shows the details of the selected item
 *
 * Tab moves between widgets, and the mouse works too. Ctrl+Q quits, and Ctrl+Z
 * suspends on Linux and macOS. */
#define TIC_IMPLEMENTATION
#include "tic.h"

#include <stdio.h>
#include <string.h>

static const char *const all_items[] = {
    "Apple", "Apricot", "Avocado", "Banana", "Blackberry", "Blueberry", "Cherry", "Clementine",
    "Coconut", "Cranberry", "Date", "Dragonfruit", "Elderberry", "Fig", "Grape", "Grapefruit",
    "Guava", "Kiwi", "Kumquat", "Lemon", "Lime", "Lychee", "Mango", "Melon", "Nectarine", "Orange",
    "Papaya", "Passionfruit", "Peach", "Pear", "Persimmon", "Pineapple", "Plum", "Pomegranate",
    "Raspberry", "Strawberry", "Tangerine", "Watermelon",
    "\xE0\xA6\x86\xE0\xA6\xAE (mango, Bengali)", "\xE8\xA5\xBF\xE7\x93\x9C (watermelon, Chinese)"
};
#define ITEM_COUNT ((int)(sizeof all_items / sizeof all_items[0]))

/* All the state is in one place. The widgets remember nothing except this. */
static struct {
    tic_ui ui;
    char search_text[64];
    tic_input search;
    tic_list list;
    tic_scroll details;
    const char *matches[ITEM_COUNT];
    int match_count;
    char picked[64];
} app;

/* A case-insensitive search for ASCII letters. Other text has to match exactly. */
static int contains(const char *text, const char *needle) {
    size_t n = strlen(needle);
    for (; *text; text++) {
        size_t i;
        for (i = 0; i < n; i++) {
            char a = text[i], b = needle[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) break;
        }
        if (i == n) return 1;
    }
    return n == 0;
}

static void update_matches(void) {
    int i;
    app.match_count = 0;
    for (i = 0; i < ITEM_COUNT; i++) {
        if (contains(all_items[i], app.search_text)) app.matches[app.match_count++] = all_items[i];
    }
}

static void draw_details(tic_buffer *s, tic_rect area, const char *item) {
    enum { LINES = 40 };
    tic_rect c = tic_ui_scroll_begin(&app.ui, area, &app.details, 0, LINES);
    int i;
    tic_textf(s, c.x, c.y, tic_style_make(TIC_BRIGHT_YELLOW, TIC_DEFAULT, TIC_BOLD), "%s", item ? item : "(nothing selected)");
    for (i = 2; i < LINES; i++) {
        tic_textf(s, c.x, c.y + i, tic_plain(), "Line %d of the details. Use the wheel, or Tab here and press the arrows.", i - 1);
    }
    tic_ui_scroll_end(&app.ui, &app.details);
}

static void draw_frame(const tic_event *ev) {
    tic_buffer *s = tic_screen();
    tic_style dim = tic_style_make(TIC_BRIGHT_BLACK, TIC_DEFAULT, 0);
    tic_style border = tic_style_make(TIC_BRIGHT_BLUE, TIC_DEFAULT, 0);

    /* Layout: a search row, the main area, and a status row. */
    int row_sizes[] = { TIC_FIXED(3), TIC_FILL(1), TIC_FIXED(1) };
    int column_sizes[] = { TIC_PERCENT(40), TIC_FILL(1) };
    tic_rect rows[3], columns[2], search_inner;
    tic_split_rows(tic_bounds(s), 0, row_sizes, 3, rows);
    tic_split_columns(rows[1], 0, column_sizes, 2, columns);

    tic_clear(s);
    tic_ui_begin(&app.ui, s, ev);

    /* Search box. */
    tic_box(s, rows[0], TIC_BORDER_ROUNDED, border);
    tic_text(s, rows[0].x + 2, rows[0].y, border, " Search ");
    search_inner = tic_rect_inset(rows[0], 1);
    if (tic_ui_input(&app.ui, search_inner, &app.search) & TIC_UI_CHANGED) {
        update_matches();
    }

    /* List of matches. */
    tic_box(s, columns[0], TIC_BORDER_ROUNDED, border);
    tic_textf(s, columns[0].x + 2, columns[0].y, border, " %d matches ", app.match_count);
    if (tic_ui_list(&app.ui, tic_rect_inset(columns[0], 1), &app.list, app.matches, app.match_count) & TIC_UI_SUBMITTED) {
        snprintf(app.picked, sizeof app.picked, "%s", app.matches[app.list.selected]);
    }

    /* Details of the selected item. */
    tic_box(s, columns[1], TIC_BORDER_ROUNDED, border);
    tic_text(s, columns[1].x + 2, columns[1].y, border, " Details ");
    draw_details(s, tic_rect_inset(columns[1], 1), app.match_count > 0 ? app.matches[app.list.selected] : NULL);

    /* Status row. */
    tic_textf(s, 1, rows[2].y, dim, "Tab: next  Enter: pick  Ctrl+Z: suspend  Ctrl+Q: quit   Picked: %s",
              app.picked[0] ? app.picked : "(none)");

    tic_ui_end(&app.ui);
    tic_present();
}

int main(void) {
    tic_event ev;

    if (tic_init() != 0) {
        fprintf(stderr, "tic_init failed: %s\n", tic_error());
        return 1;
    }
    tic_set_mouse(TIC_MOUSE_BUTTONS);

    tic_ui_init(&app.ui);
    app.ui.theme.list_selected = tic_style_make(TIC_BLACK, TIC_BRIGHT_CYAN, 0);
    app.ui.theme.list_selected_idle = tic_style_make(TIC_BLACK, TIC_WHITE, 0);
    tic_input_init(&app.search, app.search_text, sizeof app.search_text);
    app.search.placeholder = "type to filter";
    update_matches();

    memset(&ev, 0, sizeof ev);
    for (;;) {
        draw_frame(&ev);
        if (tic_poll(&ev, -1) < 0) break;
        if (ev.type == TIC_EVENT_KEY && (ev.mods & TIC_MOD_CTRL)) {
            if (ev.key == 'q') break;
            if (ev.key == 'l') tic_redraw();
            if (ev.key == 'z') tic_suspend(); /* does nothing on Windows */
        }
    }

    tic_shutdown();
    if (app.picked[0]) printf("You picked: %s\n", app.picked);
    return 0;
}
