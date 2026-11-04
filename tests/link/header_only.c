/* The declarations on their own must compile, and including them twice must be fine.
 * No system headers beyond the two the API needs should leak in. */
#include "cterm.h"
#include "cterm.h"

#ifdef _WINDOWS_
#error "windows.h leaked out of the implementation section"
#endif
#ifdef _STDIO_H
#error "stdio.h leaked out of the implementation section"
#endif

int header_only_check(void);
int header_only_check(void) {
    return CTERM_RGB(1, 2, 3) != 0;
}
