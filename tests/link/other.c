/* The normal setup, part 2: another file only needs the declarations. */
#include "cterm.h"

int other_file_width(const char *text);

int other_file_width(const char *text) {
    return cterm_text_width(text);
}
