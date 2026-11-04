/* A file that includes the header (and a system header) before asking for the
 * implementation. It needs a non-strict mode such as gnu99, which is why the README
 * says the implementation should come first. Including it again must be harmless. */
#include <stdio.h>
#include "cterm.h"
#define CTERM_IMPLEMENTATION
#include "cterm.h"
#include "cterm.h"

int main(void) {
    printf("%s", "");
    return cterm_char_width('a') == 1 ? 0 : 1;
}
