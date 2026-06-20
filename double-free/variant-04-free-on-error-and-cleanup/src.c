#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Double-free on the error path + shared cleanup — the missing early-out.
 * The function uses the common `goto cleanup` idiom: one exit that releases
 * resources. An error branch does its own local cleanup (free(buf)) and then
 * falls through to the shared `cleanup:` label, which frees buf AGAIN. Each free
 * is reasonable in isolation — the error path tidies up, the common tail tidies
 * up — but the error path should have nulled buf (or jumped past the tail free)
 * before merging. This is the single most common real-world double-free shape:
 * an extra free crept into an error branch that still flows into the canonical
 * cleanup. Fires only when the error condition is hit (input[0] == '!'). */
int vuln(const char *input) {
    char *buf = (char *)malloc(32);
    int rc = 0;
    strncpy(buf, input, 31);
    buf[31] = '\0';

    if (buf[0] == '!') {       /* an "error" condition */
        free(buf);             /* local cleanup on the error path... */
        rc = -1;
        goto cleanup;          /* ...but we fall into the shared cleanup tail, */
    }
    rc = (int)(unsigned char)buf[0];

cleanup:
    free(buf);                 /* ...which frees buf a second time on error */
    return rc;
}

int main(int argc, char **argv) {
    if (argc > 1) printf("%d\n", vuln(argv[1]));
    return 0;
}
