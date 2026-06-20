#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Double-free disguised as defensive code — the guard guards nothing.
 * The idiom `if (p) free(p);` LOOKS safe, and developers add it believing it
 * prevents a double-free. It does not: free() does not change p. After the first
 * guarded free, p still holds the same (now dangling) non-NULL address, so the
 * SECOND `if (p) free(p)` passes its check and frees the chunk again. The check
 * is decorative — the only thing that makes the guard real is nulling the
 * pointer after freeing it (p = NULL), which turns the second free into the
 * no-op free(NULL). The bug is that the disguise reads as protection. */
int vuln(const char *input) {
    char *buf = (char *)malloc(32);
    strncpy(buf, input, 31);
    buf[31] = '\0';
    int r = (int)(unsigned char)buf[0];

    if (buf) free(buf);    /* "defensive" free #1 — but buf is never nulled */
    /* ...later, a second cleanup site repeats the same defensive pattern... */
    if (buf) free(buf);    /* guard still true (buf unchanged) -> double free */
    return r;
}

int main(int argc, char **argv) {
    if (argc > 1) printf("%d\n", vuln(argv[1]));
    return 0;
}
