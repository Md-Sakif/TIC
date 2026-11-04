/* The normal setup, part 1: this file holds the implementation. */
#define CTERM_IMPLEMENTATION
#include "cterm.h"

#ifdef __cplusplus
extern "C" {
#endif
int other_file_width(const char *text);
#ifdef __cplusplus
}
#endif

int main(void) {
    cterm_buffer *b = cterm_buffer_new(10, 1);
    if (!b) return 1;
    cterm_text(b, 0, 0, cterm_plain(), "hi");
    cterm_buffer_free(b);
    return other_file_width("ab") == 2 ? 0 : 1;
}
