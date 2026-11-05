/* step8.c - a search box to filter the list.
 *
 * Part of the system monitor tutorial: see README.md in this folder. */

#define CTERM_IMPLEMENTATION
#include "cterm.h"
#include "stats.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The latest numbers from the system. */
static double cpu_percent = 0.0;
static unsigned long long memory_used = 0, memory_total = 0;

/* The last two minutes of CPU usage, oldest first. */
#define HISTORY_SIZE 120
static double cpu_history[HISTORY_SIZE];

/* The running programs, biggest memory user first. */
#define MAX_PROCESSES 4096
static stats_process processes[MAX_PROCESSES];
static int process_count = 0;

/* The ones that match the search box. These point into `processes`. */
static const stats_process *matches[MAX_PROCESSES];
static int match_count = 0;

/* What the widgets remember between frames. */
static cterm_ui ui;
static cterm_list process_list;
static cterm_input search;
static char search_text[64];

/* Tells qsort how to order two processes: more memory comes first. */
static int compare_by_memory(const void *a, const void *b) {
    const stats_process *first = (const stats_process *)a;
    const stats_process *second = (const stats_process *)b;
    if (first->memory > second->memory) return -1;
    if (first->memory < second->memory) return 1;
    return 0;
}

/* Returns 1 if `text` contains `wanted`, treating capital and small letters as equal. */
static int contains(const char *text, const char *wanted) {
    size_t length = strlen(wanted);
    if (length == 0) return 1;
    for (; *text; text++) {
        size_t i;
        for (i = 0; i < length; i++) {
            if (tolower((unsigned char)text[i]) != tolower((unsigned char)wanted[i])) break;
        }
        if (i == length) return 1;
    }
    return 0;
}

/* Rebuilds the list of processes that match what's in the search box. */
static void update_matches(void) {
    int i;
    match_count = 0;
    for (i = 0; i < process_count; i++) {
        if (contains(processes[i].name, search_text)) matches[match_count++] = &processes[i];
    }
}

static void update_stats(void) {
    int i;

    cpu_percent = stats_cpu_percent();
    stats_memory(&memory_used, &memory_total);

    for (i = 0; i < HISTORY_SIZE - 1; i++) cpu_history[i] = cpu_history[i + 1];
    cpu_history[HISTORY_SIZE - 1] = cpu_percent;

    process_count = stats_processes(processes, MAX_PROCESSES);
    qsort(processes, (size_t)process_count, sizeof processes[0], compare_by_memory);
    update_matches();
}

/* Writes a number of bytes in a readable way, such as "3.2 GB" or "512 MB". */
static void format_bytes(char *out, size_t size, unsigned long long bytes) {
    double gigabytes = (double)bytes / (1024.0 * 1024.0 * 1024.0);
    if (gigabytes >= 1.0) snprintf(out, size, "%.1f GB", gigabytes);
    else snprintf(out, size, "%llu MB", bytes / (1024 * 1024));
}

static void draw_panel(cterm_buffer *screen, cterm_rect area, const char *title) {
    cterm_style border = cterm_style_make(CTERM_BRIGHT_BLUE, CTERM_DEFAULT, 0);
    cterm_box(screen, area, CTERM_BORDER_ROUNDED, border);
    cterm_textf(screen, area.x + 2, area.y, border, " %s ", title);
}

/* Draws a bar like this, where `percent` says how much is filled in:  #######---- */
static void draw_bar(cterm_buffer *screen, int x, int y, int width, double percent) {
    int filled = (int)(width * percent / 100.0 + 0.5);
    cterm_color color = percent < 60.0 ? CTERM_GREEN : percent < 85.0 ? CTERM_YELLOW : CTERM_RED;

    cterm_fill(screen, cterm_rect_make(x, y, filled, 1), 0x2588, /* a solid block */
               cterm_style_make(color, CTERM_DEFAULT, 0));
    cterm_fill(screen, cterm_rect_make(x + filled, y, width - filled, 1), 0x2591, /* a light block */
               cterm_style_make(CTERM_BRIGHT_BLACK, CTERM_DEFAULT, 0));
}

/* Draws the history as a graph, with the newest value at the right-hand edge. */
static void draw_graph(cterm_buffer *screen, cterm_rect area) {
    cterm_style style = cterm_style_make(CTERM_BRIGHT_GREEN, CTERM_DEFAULT, 0);
    int column, level;

    for (column = 0; column < area.w && column < HISTORY_SIZE; column++) {
        double value = cpu_history[HISTORY_SIZE - 1 - column];
        int x = area.x + area.w - 1 - column;

        /* Each cell can show 8 heights, so the column's height is counted in eighths. */
        int eighths = (int)(value / 100.0 * area.h * 8.0 + 0.5);

        for (level = 0; level < area.h; level++) {
            int y = area.y + area.h - 1 - level;
            int here = eighths - level * 8;
            if (here >= 8) cterm_put(screen, x, y, 0x2588, style);        /* a full block */
            else if (here > 0) cterm_put(screen, x, y, 0x2580 + (uint32_t)here, style); /* a partial block */
        }
    }
}

static void draw_cpu(cterm_buffer *screen, cterm_rect area) {
    cterm_rect inside = cterm_rect_inset(area, 1);
    cterm_rect graph = cterm_rect_make(inside.x, inside.y + 1, inside.w, inside.h - 1);
    draw_panel(screen, area, "CPU");
    cterm_textf(screen, inside.x, inside.y, cterm_plain(), "%5.1f%%", cpu_percent);
    draw_bar(screen, inside.x + 7, inside.y, inside.w - 7, cpu_percent);
    draw_graph(screen, graph);
}

static void draw_memory(cterm_buffer *screen, cterm_rect area) {
    cterm_rect inside = cterm_rect_inset(area, 1);
    double percent = memory_total > 0 ? 100.0 * (double)memory_used / (double)memory_total : 0.0;
    char used[32], total[32];
    format_bytes(used, sizeof used, memory_used);
    format_bytes(total, sizeof total, memory_total);
    draw_panel(screen, area, "Memory");
    draw_bar(screen, inside.x, inside.y, inside.w, percent);
    cterm_textf(screen, inside.x, inside.y + 1, cterm_plain(), "%s of %s", used, total);
}

/* The list calls this to get the text of each row it shows. */
static const char *process_row(int index, void *user_data) {
    static char row[128];
    char memory[32];
    const stats_process *process = matches[index];
    (void)user_data; /* we don't need this: it says so on purpose, to avoid a warning */
    format_bytes(memory, sizeof memory, process->memory);
    snprintf(row, sizeof row, "%7d  %9s  %s", process->pid, memory, process->name);
    return row;
}

static void draw_processes(cterm_buffer *screen, cterm_rect area) {
    cterm_style heading = cterm_style_make(CTERM_DEFAULT, CTERM_DEFAULT, CTERM_BOLD);
    cterm_rect inside = cterm_rect_inset(area, 1);
    cterm_rect search_row = cterm_rect_make(inside.x + 8, inside.y, inside.w - 8, 1);
    cterm_rect rows = cterm_rect_make(inside.x, inside.y + 2, inside.w, inside.h - 2);

    draw_panel(screen, area, "Processes");

    cterm_text(screen, inside.x, inside.y, heading, "Search:");
    if (cterm_ui_input(&ui, search_row, &search) & CTERM_UI_CHANGED) {
        update_matches();
    }

    cterm_textf(screen, inside.x, inside.y + 1, heading, "%7s  %9s  %s", "PID", "MEMORY", "NAME");
    cterm_ui_list_fn(&ui, rows, &process_list, match_count, process_row, NULL);
}

static void draw(const cterm_event *event) {
    cterm_buffer *screen = cterm_screen();
    cterm_style title_style = cterm_style_make(CTERM_BLACK, CTERM_CYAN, CTERM_BOLD);
    cterm_style help_style = cterm_style_make(CTERM_BRIGHT_BLACK, CTERM_DEFAULT, 0);

    int row_sizes[] = { CTERM_FIXED(1), CTERM_FIXED(7), CTERM_FILL(1), CTERM_FIXED(1) };
    int top_sizes[] = { CTERM_FILL(2), CTERM_FILL(1) };
    cterm_rect rows[4], top[2];

    cterm_split_rows(cterm_bounds(screen), 0, row_sizes, 4, rows);
    cterm_split_columns(rows[1], 0, top_sizes, 2, top);

    cterm_clear(screen);
    cterm_ui_begin(&ui, screen, event);

    cterm_fill(screen, rows[0], ' ', title_style);
    cterm_text(screen, 1, rows[0].y, title_style, "System Monitor");

    draw_cpu(screen, top[0]);
    draw_memory(screen, top[1]);
    draw_processes(screen, rows[2]);

    cterm_text(screen, 1, rows[3].y, help_style, "Tab: switch box  Up/Down: move  Ctrl+Q: quit");

    cterm_ui_end(&ui);
    cterm_present();
}

int main(void) {
    time_t last_update = 0;
    cterm_event event;

    if (cterm_init() != 0) {
        fprintf(stderr, "Could not start: %s\n", cterm_error());
        return 1;
    }
    cterm_ui_init(&ui);
    cterm_input_init(&search, search_text, sizeof search_text);
    search.placeholder = "type part of a name";
    memset(&event, 0, sizeof event);

    for (;;) {
        int result;

        if (time(NULL) != last_update) {
            update_stats();
            last_update = time(NULL);
        }

        draw(&event);

        result = cterm_poll(&event, 250);
        if (result < 0) break;
        if (event.type == CTERM_EVENT_KEY && event.key == 'q' && (event.mods & CTERM_MOD_CTRL)) break;
    }

    cterm_shutdown();
    return 0;
}
