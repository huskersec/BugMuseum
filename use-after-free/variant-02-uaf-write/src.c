#include <stdlib.h>
#include <string.h>

/* Use-after-free (write).
 * buf is freed, then written through the dangling pointer. The write lands in a
 * chunk the allocator has already reclaimed — corrupting a freelist link, or
 * whatever a later allocation has since placed there. More dangerous than a UAF
 * read: it is a write primitive into reused memory. */
void vuln(const char *input) {
    char *buf = (char *)malloc(32);
    free(buf);                      /* lifetime ends... */
    strncpy(buf, input, 31);        /* ...then we write into the freed chunk */
    buf[31] = '\0';
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
