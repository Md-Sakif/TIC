/* TIC_STATIC: everything is private to this file, and calling only part of the API
 * must not cause "unused function" warnings. */
#define TIC_STATIC
#define TIC_IMPLEMENTATION
#include "tic.h"

int main(void) {
    return tic_char_width('a') == 1 ? 0 : 1;
}
