/* The normal setup, part 1: this file holds the implementation. */
#define TIC_IMPLEMENTATION
#include "tic.h"

#ifdef __cplusplus
extern "C" {
#endif
int other_file_width(const char *text);
#ifdef __cplusplus
}
#endif

int main(void) {
    tic_buffer *b = tic_buffer_new(10, 1);
    if (!b) return 1;
    tic_text(b, 0, 0, tic_plain(), "hi");
    tic_buffer_free(b);
    return other_file_width("ab") == 2 ? 0 : 1;
}
