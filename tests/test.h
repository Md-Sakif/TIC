/* test.h - the tiny test framework used by every file in tests/.
 *
 * A test file looks like this:
 *
 *     #define CTERM_IMPLEMENTATION
 *     #include "cterm.h"
 *     #include "test.h"
 *
 *     static void test_something(void) {
 *         CHECK(1 + 1 == 2);
 *         CHECK_INT(cterm_char_width('a'), 1);
 *     }
 *
 *     int main(void) {
 *         RUN(test_something);
 *         return test_summary();
 *     }
 *
 * Including the implementation gives tests access to internal `cterm__` functions,
 * so the parser, renderer and widgets can be tested without a terminal.
 * A failed CHECK reports the problem and the test carries on, so one run shows
 * every failure.
 */
#ifndef CTERM_TEST_H
#define CTERM_TEST_H

#include <stdio.h>
#include <string.h>

static int test_failures = 0;
static int test_checks = 0;
static int test_count = 0;

#define CHECK(condition)                                                    \
    do {                                                                    \
        test_checks++;                                                      \
        if (!(condition)) {                                                 \
            test_failures++;                                                \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);   \
        }                                                                   \
    } while (0)

#define CHECK_INT(actual, expected)                                         \
    do {                                                                    \
        long long test_a = (long long)(actual);                             \
        long long test_e = (long long)(expected);                           \
        test_checks++;                                                      \
        if (test_a != test_e) {                                             \
            test_failures++;                                                \
            printf("  FAIL %s:%d: %s is %lld, expected %lld\n", __FILE__,   \
                   __LINE__, #actual, test_a, test_e);                      \
        }                                                                   \
    } while (0)

#define CHECK_STR(actual, expected)                                         \
    do {                                                                    \
        const char *test_a = (actual);                                      \
        const char *test_e = (expected);                                    \
        test_checks++;                                                      \
        if (strcmp(test_a, test_e) != 0) {                                  \
            test_failures++;                                                \
            printf("  FAIL %s:%d: %s is \"%s\", expected \"%s\"\n",         \
                   __FILE__, __LINE__, #actual, test_a, test_e);            \
        }                                                                   \
    } while (0)

#define RUN(test_function)                                                  \
    do {                                                                    \
        test_count++;                                                       \
        test_function();                                                    \
    } while (0)

static int test_summary(void) {
    printf("%s: %d tests, %d checks, %d failed\n", test_failures ? "FAILED" : "ok",
           test_count, test_checks, test_failures);
    return test_failures ? 1 : 0;
}

#endif /* CTERM_TEST_H */
