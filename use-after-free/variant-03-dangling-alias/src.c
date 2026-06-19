#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Use-after-free via a retained alias.
 * The allocation is freed through `buf`, but a second pointer (`g_last`) still
 * holds the same address and is used afterward. The free and the use go through
 * DIFFERENT names — so reading only the lines that mention `buf`, you'd never
 * see the bug. Caches, intrusive lists, and "last seen" globals create exactly
 * this dangling-alias situation. */
static char *g_last;

void vuln(const char *input) {
    char *buf = (char *)malloc(32);
    strncpy(buf, input, 31);
    buf[31] = '\0';
    g_last = buf;                 /* retain an alias to the same allocation */
    free(buf);                    /* freed via buf... */
    printf("%s\n", g_last);       /* ...used via the dangling alias */
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
