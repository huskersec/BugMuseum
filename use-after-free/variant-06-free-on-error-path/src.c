#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Use-after-free on a control-flow path.
 * The free is correct cleanup; the slip is using buf AFTER cleanup on one path.
 * The "empty input" case jumps to cleanup (which frees buf), and then the
 * rejection log dereferences buf — a use-after-free that fires only on the
 * reject path. In isolation the free looks right; the defect is the post-free
 * use that the branch structure lets through. (There's exactly one free here —
 * no double-free; the bug is purely temporal.) */
int vuln(const char *input) {
    char *buf = (char *)malloc(32);
    int rc = 0;
    strncpy(buf, input, 31);
    buf[31] = '\0';

    if (buf[0] == '\0')        /* empty input: reject */
        goto cleanup;

    rc = (int)strlen(buf);

cleanup:
    free(buf);                 /* release buf */
    if (rc == 0)
        printf("rejected: '%s'\n", buf);   /* use-after-free on the reject path */
    return rc;
}

int main(int argc, char **argv) {
    if (argc > 1) return vuln(argv[1]);   /* try an empty arg: vuln.exe "" */
    return 0;
}
