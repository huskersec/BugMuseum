#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Use-after-free of an owned sub-object.
 * A Buffer owns a heap `data` allocation; buffer_free() destroys both. The
 * caller saved a pointer INTO that owned allocation, then destroyed the owner,
 * then used the saved pointer. Freeing a container frees its parts, so the
 * saved child pointer dangles. The free is inside the destructor — not visible
 * at the use site — which is what makes this one non-local. */
struct Buffer { char *data; size_t len; };

static void buffer_free(struct Buffer *b) {
    free(b->data);
    free(b);
}

void vuln(const char *input) {
    struct Buffer *b = (struct Buffer *)malloc(sizeof *b);
    b->data = (char *)malloc(32);
    strncpy(b->data, input, 31);
    b->data[31] = '\0';
    b->len = strlen(b->data);

    char *p = b->data;            /* save a pointer to the owned data */
    buffer_free(b);               /* destroys b AND b->data */
    printf("%s\n", p);            /* dangling: p pointed into b's owned allocation */
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
