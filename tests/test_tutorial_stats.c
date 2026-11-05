/* Checks the tutorial's stats.h gives sensible answers on the real OS. It needs no
 * terminal, so this also proves the Windows and macOS versions work in CI. */
#define CTERM_IMPLEMENTATION
#include "cterm.h"
#include "stats.h"
#include "test.h"

#define MAX_PROCESSES 8192
static stats_process processes[MAX_PROCESSES];

static void test_cpu(void) {
    double first = stats_cpu_percent();
    volatile unsigned long spin = 0;
    double second;
    unsigned long i;
    CHECK(first == 0.0); /* nothing to compare with yet */

    /* Do some work, so that time passes and the counters move. */
    for (i = 0; i < 100000000UL; i++) spin += i;
    second = stats_cpu_percent();
    CHECK(second >= 0.0);
    CHECK(second <= 100.0);
}

static void test_memory(void) {
    unsigned long long used = 0, total = 0;
    stats_memory(&used, &total);
    CHECK(total > 0);
    CHECK(used > 0);
    CHECK(used <= total);
    printf("  memory: %llu of %llu MB\n", used / (1024 * 1024), total / (1024 * 1024));
}

static void test_processes(void) {
    int count = stats_processes(processes, MAX_PROCESSES), i;
    int named = 0, with_memory = 0;
    CHECK(count > 0);
    for (i = 0; i < count; i++) {
        if (processes[i].name[0]) named++;
        if (processes[i].memory > 0) with_memory++;
        CHECK(strlen(processes[i].name) < sizeof processes[i].name);
    }
    CHECK(named > 0);
    /* This test itself is running, so at least one program must report memory. */
    CHECK(with_memory > 0);
    printf("  processes: %d, %d with memory figures\n", count, with_memory);
}

static void test_processes_respects_the_limit(void) {
    stats_process few[2];
    CHECK(stats_processes(few, 2) <= 2);
}

int main(void) {
    RUN(test_cpu);
    RUN(test_memory);
    RUN(test_processes);
    RUN(test_processes_respects_the_limit);
    return test_summary();
}
