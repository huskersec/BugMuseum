#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Classic use-after-free (read) — the baseline.
 * buf is freed, then read through the dangling pointer. Nothing is written out
 * of bounds; the bug is temporal — the pointer outlives the object. After
 * free(), the chunk may be handed to another allocation, so the read returns
 * stale or attacker-influenced bytes (or faults under Full Page Heap). A UAF
 * read often does NOT crash; it quietly yields wrong data, which is worse. */
int vuln(const char *input) {
    char *buf = (char *)malloc(32);
    strncpy(buf, input, 31);
    buf[31] = '\0';
    free(buf);                            /* the object's lifetime ends here */
    return (int)(unsigned char)buf[0];    /* ...but we read it afterwards */
}

int main(int argc, char **argv) {
    if (argc > 1) printf("%d\n", vuln(argv[1]));
    return 0;
}
