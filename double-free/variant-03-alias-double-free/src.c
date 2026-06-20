#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Double-free via an alias — freed through two different pointers.
 * vuln keeps the most-recently-processed buffer in a global (g_last) "for reuse"
 * and, on each call, frees the previous one before replacing it. But it ALSO
 * frees buf at the end of its own work. So buf is owned by two homes that hold
 * the same address: the local `buf` and the global `g_last`. The double free
 * straddles two calls — the first call frees buf and stashes its address in
 * g_last; the NEXT call's `free(g_last)` releases that already-freed chunk.
 * Each free site looks like it owns its pointer; the defect is that the cache is
 * treated as an independent owner instead of being cleared. Single ownership —
 * and nulling the alias — is the fix. */
static char *g_last;   /* alias kept across calls — a second "owner" */

int vuln(const char *input) {
    char *buf = (char *)malloc(32);
    strncpy(buf, input, 31);
    buf[31] = '\0';
    int r = (int)(unsigned char)buf[0];

    free(g_last);          /* release the previous buffer before replacing it */
    g_last = buf;          /* remember this one as the new "last" (alias) */
    free(buf);             /* but we free it here too -> next call's
                              free(g_last) frees this same chunk again */
    return r;
}

int main(int argc, char **argv) {
    /* two calls: the second's free(g_last) double-frees the first's buffer */
    if (argc > 1) printf("%d\n", vuln(argv[1]));
    if (argc > 2) printf("%d\n", vuln(argv[2]));
    return 0;
}
