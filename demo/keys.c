/* keys.c - shows the raw bytes your terminal sends for each key. Press q to quit.
 *
 * Useful when a key doesn't behave the way you expect. */
#define CTERM_IMPLEMENTATION
#include "cterm.h"

#include <stdio.h>

int main(void) {
    unsigned char bytes[64];
    int running = 1;

    if (cterm_init() != 0) {
        fprintf(stderr, "cterm_init failed: %s\n", cterm_error());
        return 1;
    }

    printf("Press keys to see their bytes. Press q to quit.\r\n");
    fflush(stdout);

    while (running) {
        int i, n = cterm_read(bytes, (int)sizeof bytes);
        if (n < 0) break;
        if (n == 0) continue;
        for (i = 0; i < n; i++) {
            printf("%02x ", bytes[i]);
            if (bytes[i] == 'q') running = 0;
        }
        printf("\r\n");
        fflush(stdout);
    }

    cterm_shutdown();
    return 0;
}
