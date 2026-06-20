#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Double-free of a shared owned member — two owners, one buffer.
 * A Blob owns its ->data buffer; blob_destroy() frees ->data and nulls the
 * field. Two Blobs are created, but for "efficiency" the second is made to share
 * the FIRST blob's buffer (b.data = a.data) instead of getting its own copy. Now
 * both Blobs believe they own that buffer, so destroying each frees it — twice.
 * The destructor is correct for a Blob that owns its data; the defect is that
 * ownership was shared, not copied. Note that blob_destroy nulls THE FIELD it
 * was given (a.data, then b.data) — but those are two separate pointer variables
 * holding the same address, so nulling one does not protect the other. Clearing
 * a freed pointer only helps the owner you cleared; it cannot save an alias. */
typedef struct { char *data; size_t len; } Blob;   /* owns ->data */

static void blob_destroy(Blob *b) {
    free(b->data);             /* a Blob owns its data and frees it */
    b->data = NULL;            /* clears THIS owner's field — not any alias */
}

int vuln(const char *input) {
    Blob a, b;
    a.len = 32;
    a.data = (char *)malloc(a.len);
    strncpy(a.data, input, a.len - 1);
    a.data[a.len - 1] = '\0';

    b.len  = a.len;
    b.data = a.data;           /* shares a's buffer instead of copying it */

    int r = (int)(unsigned char)a.data[0];
    blob_destroy(&a);          /* frees the shared buffer */
    blob_destroy(&b);          /* frees the SAME buffer again -> double free */
    return r;
}

int main(int argc, char **argv) {
    if (argc > 1) printf("%d\n", vuln(argv[1]));
    return 0;
}
