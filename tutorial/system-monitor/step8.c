/* step8.c - a search box to filter the list.
 *
 * Part of the system monitor tutorial: see README.md in this folder. */

#define TIC_IMPLEMENTATION
#include "tic.h"
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
static tic_ui ui;
static tic_list process_list;
static tic_input search;
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

static void draw_panel(tic_buffer *screen, tic_rect area, const char *title) {
    tic_style border = tic_style_make(TIC_BRIGHT_BLUE, TIC_DEFAULT, 0);
    tic_box(screen, area, TIC_BORDER_ROUNDED, border);
    tic_textf(screen, area.x + 2, area.y, border, " %s ", title);
}

/* Draws a bar like this, where `percent` says how much is filled in:  #######---- */
static void draw_bar(tic_buffer *screen, int x, int y, int width, double percent) {
    int filled = (int)(width * percent / 100.0 + 0.5);
    tic_color color = percent < 60.0 ? TIC_GREEN : percent < 85.0 ? TIC_YELLOW : TIC_RED;

    tic_fill(screen, tic_rect_make(x, y, filled, 1), 0x2588, /* a solid block */
             tic_style_make(color, TIC_DEFAULT, 0));
    tic_fill(screen, tic_rect_make(x + filled, y, width - filled, 1), 0x2591, /* a light block */
             tic_style_make(TIC_BRIGHT_BLACK, TIC_DEFAULT, 0));
}

/* Draws the history as a graph, with the newest value at the right-hand edge. */
static void draw_graph(tic_buffer *screen, tic_rect area) {
    tic_style style = tic_style_make(TIC_BRIGHT_GREEN, TIC_DEFAULT, 0);
    int column, level;

    for (column = 0; column < area.w && column < HISTORY_SIZE; column++) {
        double value = cpu_history[HISTORY_SIZE - 1 - column];
        int x = area.x + area.w - 1 - column;

        /* Each cell can show 8 heights, so the column's height is counted in eighths. */
        int eighths = (int)(value / 100.0 * area.h * 8.0 + 0.5);

        for (level = 0; level < area.h; level++) {
            int y = area.y + area.h - 1 - level;
            int here = eighths - level * 8;
            if (here >= 8) tic_put(screen, x, y, 0x2588, style);        /* a full block */
            else if (here > 0) tic_put(screen, x, y, 0x2580 + (uint32_t)here, style); /* a partial block */
        }
    }
}

static void draw_cpu(tic_buffer *screen, tic_rect area) {
    tic_rect inside = tic_rect_inset(area, 1);
    tic_rect graph = tic_rect_make(inside.x, inside.y + 1, inside.w, inside.h - 1);
    draw_panel(screen, area, "CPU");
    tic_textf(screen, inside.x, inside.y, tic_plain(), "%5.1f%%", cpu_percent);
    draw_bar(screen, inside.x + 7, inside.y, inside.w - 7, cpu_percent);
    draw_graph(screen, graph);
}

static void draw_memory(tic_buffer *screen, tic_rect area) {
    tic_rect inside = tic_rect_inset(area, 1);
    double percent = memory_total > 0 ? 100.0 * (double)memory_used / (double)memory_total : 0.0;
    char used[32], total[32];
    format_bytes(used, sizeof used, memory_used);
    format_bytes(total, sizeof total, memory_total);
    draw_panel(screen, area, "Memory");
    draw_bar(screen, inside.x, inside.y, inside.w, percent);
    tic_textf(screen, inside.x, inside.y + 1, tic_plain(), "%s of %s", used, total);
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

static void draw_processes(tic_buffer *screen, tic_rect area) {
    tic_style heading = tic_style_make(TIC_DEFAULT, TIC_DEFAULT, TIC_BOLD);
    tic_rect inside = tic_rect_inset(area, 1);
    tic_rect search_row = tic_rect_make(inside.x + 8, inside.y, inside.w - 8, 1);
    tic_rect rows = tic_rect_make(inside.x, inside.y + 2, inside.w, inside.h - 2);

    draw_panel(screen, area, "Processes");

    tic_text(screen, inside.x, inside.y, heading, "Search:");
    if (tic_ui_input(&ui, search_row, &search) & TIC_UI_CHANGED) {
        update_matches();
    }

    tic_textf(screen, inside.x, inside.y + 1, heading, "%7s  %9s  %s", "PID", "MEMORY", "NAME");
    tic_ui_list_fn(&ui, rows, &process_list, match_count, process_row, NULL);
}

static void draw(const tic_event *event) {
    tic_buffer *screen = tic_screen();
    tic_style title_style = tic_style_make(TIC_BLACK, TIC_CYAN, TIC_BOLD);
    tic_style help_style = tic_style_make(TIC_BRIGHT_BLACK, TIC_DEFAULT, 0);

    int row_sizes[] = { TIC_FIXED(1), TIC_FIXED(7), TIC_FILL(1), TIC_FIXED(1) };
    int top_sizes[] = { TIC_FILL(2), TIC_FILL(1) };
    tic_rect rows[4], top[2];

    tic_split_rows(tic_bounds(screen), 0, row_sizes, 4, rows);
    tic_split_columns(rows[1], 0, top_sizes, 2, top);

    tic_clear(screen);
    tic_ui_begin(&ui, screen, event);

    tic_fill(screen, rows[0], ' ', title_style);
    tic_text(screen, 1, rows[0].y, title_style, "System Monitor");

    draw_cpu(screen, top[0]);
    draw_memory(screen, top[1]);
    draw_processes(screen, rows[2]);

    tic_text(screen, 1, rows[3].y, help_style, "Tab: switch box  Up/Down: move  Ctrl+Q: quit");

    tic_ui_end(&ui);
    tic_present();
}

int main(void) {
    time_t last_update = 0;
    tic_event event;

    if (tic_init() != 0) {
        fprintf(stderr, "Could not start: %s\n", tic_error());
        return 1;
    }
    tic_ui_init(&ui);
    tic_input_init(&search, search_text, sizeof search_text);
    search.placeholder = "type part of a name";
    memset(&event, 0, sizeof event);

    for (;;) {
        int result;

        if (time(NULL) != last_update) {
            update_stats();
            last_update = time(NULL);
        }

        draw(&event);

        result = tic_poll(&event, 250);
        if (result < 0) break;
        if (event.type == TIC_EVENT_KEY && event.key == 'q' && (event.mods & TIC_MOD_CTRL)) break;
    }

    tic_shutdown();
    return 0;
}
