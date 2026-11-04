/* CTERM_STATIC: everything is private to this file, and calling only part of the API
 * must not cause "unused function" warnings. */
#define CTERM_STATIC
#define CTERM_IMPLEMENTATION
#include "cterm.h"

int main(void) {
    return cterm_char_width('a') == 1 ? 0 : 1;
}
