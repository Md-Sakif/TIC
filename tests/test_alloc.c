/* Custom allocators, and proof that everything allocated gets freed. Sanitizers do
 * this on Linux, but this test also works on systems without them. It also fails
 * allocations on purpose, which is otherwise almost impossible to test. */
#include <stddef.h> /* only for size_t: it doesn't count as "another header" for the
                       rule that the implementation comes first, because it comes with
                       the compiler rather than from libc */

static void *test_malloc(size_t size);
static void *test_realloc(void *ptr, size_t size);
static void test_free(void *ptr);

#define TIC_MALLOC(size)       test_malloc(size)
#define TIC_REALLOC(ptr, size) test_realloc(ptr, size)
#define TIC_FREE(ptr)          test_free(ptr)
#define TIC_IMPLEMENTATION
#include "tic.h"
#include "test.h"

static long live_allocations = 0;
static long total_allocations = 0;
static long fail_after = -1; /* -1 means never fail */

static int should_fail(void) {
    if (fail_after < 0) return 0;
    if (fail_after == 0) return 1;
    fail_after--;
    return 0;
}

static void *test_malloc(size_t size) {
    if (should_fail()) return NULL;
    live_allocations++;
    total_allocations++;
    return malloc(size);
}

static void *test_realloc(void *ptr, size_t size) {
    if (should_fail()) return NULL;
    if (!ptr) {
        live_allocations++;
        total_allocations++;
    }
    return realloc(ptr, size);
}

static void test_free(void *ptr) {
    if (ptr) live_allocations--;
    free(ptr);
}

static void exercise_everything(void) {
    tic_buffer *front, *back;
    tic__sb out;
    tic__render_state state;
    tic__parser parser;
    tic_event ev;

    back = tic_buffer_new(20, 5);
    front = tic_buffer_new(20, 5);
    if (back && front) {
        tic_text(back, 0, 0, tic_plain(), "hello");
        tic_textf(back, 0, 1, tic_plain(), "%s", "a long string that goes past the stack buffer "
            "inside tic_textf, a long string that goes past the stack buffer inside tic_textf, "
            "a long string that goes past the stack buffer inside tic_textf, a long string that "
            "goes past the stack buffer inside tic_textf, and then some");
        memset(&out, 0, sizeof out);
        tic__render_state_reset(&state);
        tic__render(&out, front, back, &state, TIC_COLORS_RGB, 1);
        tic__sb_free(&out);
        tic_buffer_resize(back, 40, 10);
        tic_buffer_resize(front, 40, 10);
    }
    tic_buffer_free(back);
    tic_buffer_free(front);

    memset(&parser, 0, sizeof parser);
    tic__parser_feed(&parser, (const unsigned char *)"\x1b[200~pasted\x1b[201~", 18);
    while (tic__parser_next(&parser, &ev, 1)) {}
    tic__parser_free(&parser);
}

static void test_everything_is_freed(void) {
    fail_after = -1;
    exercise_everything();
    CHECK(total_allocations > 0);
    CHECK_INT(live_allocations, 0);
}

static void test_out_of_memory_never_crashes(void) {
    /* Fail the 1st allocation, then the 2nd, then the 3rd, and so on. Each time it must
     * cope without crashing or leaking. */
    long i;
    for (i = 0; i < 40; i++) {
        fail_after = i;
        exercise_everything();
        fail_after = -1;
        CHECK_INT(live_allocations, 0);
    }
}

static void test_resize_failure_keeps_buffer(void) {
    tic_buffer *b = tic_buffer_new(4, 1);
    tic_text(b, 0, 0, tic_plain(), "keep");
    fail_after = 0;
    CHECK_INT(tic_buffer_resize(b, 100, 100), -1);
    fail_after = -1;
    CHECK_INT(b->width, 4);
    CHECK_INT(tic__cell_at(b, 0, 0)->ch, 'k');
    CHECK_STR(tic_error(), "out of memory");
    tic_buffer_free(b);

    fail_after = 0;
    CHECK(tic_buffer_new(4, 1) == NULL);
    fail_after = 1; /* the struct succeeds and the cells fail */
    CHECK(tic_buffer_new(4, 1) == NULL);
    fail_after = -1;
    CHECK_INT(live_allocations, 0);
}

int main(void) {
    RUN(test_everything_is_freed);
    RUN(test_out_of_memory_never_crashes);
    RUN(test_resize_failure_keeps_buffer);
    return test_summary();
}
