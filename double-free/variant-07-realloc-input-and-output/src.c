#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Double-free around realloc — freeing both the input and the output.
 * realloc(p, n) releases p (possibly MOVING the data to a fresh block) and
 * returns the new pointer. After a successful realloc, p is dead — it must NOT
 * be freed, because realloc already did. Here vuln frees BOTH the original
 * pointer and the realloc result. If realloc moved the block, free(buf) frees an
 * already-released chunk; if it grew in place (buf == grown), the two frees hit
 * the same chunk outright. Either way it is a double free. realloc is the one
 * call that frees its argument as a side effect, which is exactly why "free the
 * old, free the new" is such a common mistake. (The NULL branch below is the
 * correct case: on failure realloc keeps the original, so freeing buf there is
 * right — contrast it with the success path.) */
int vuln(const char *input) {
    char *buf = (char *)malloc(16);
    strncpy(buf, input, 15);
    buf[15] = '\0';

    char *grown = (char *)realloc(buf, 64);    /* may free + move buf */
    if (!grown) { free(buf); return -1; }      /* failure: realloc kept buf (ok) */

    int r = (int)(unsigned char)grown[0];
    free(buf);                 /* WRONG: realloc already released buf */
    free(grown);               /* the real owner of the live block */
    return r;
}

int main(int argc, char **argv) {
    if (argc > 1) printf("%d\n", vuln(argv[1]));
    return 0;
}
