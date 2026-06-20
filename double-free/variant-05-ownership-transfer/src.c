#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Double-free across a function boundary — ownership was transferred.
 * consume() takes ownership of its argument and frees it (a common contract:
 * "I'll dispose of what you hand me"). vuln passes buf to consume(), which frees
 * it — then vuln frees buf AGAIN in its own cleanup, unaware the callee already
 * took ownership. Each free is locally correct: consume frees what it owns,
 * vuln frees what it allocated. The bug is the disagreement about WHO owns the
 * chunk after the call — a transfer the caller didn't account for. The tell
 * spans two bodies; in vuln's own disassembly it reads as `call consume` then
 * `call free` on the same pointer with no reallocation between. */
static int consume(char *p) {
    int r = (int)(unsigned char)p[0];
    free(p);                   /* consume() OWNS p and releases it */
    return r;
}

int vuln(const char *input) {
    char *buf = (char *)malloc(32);
    strncpy(buf, input, 31);
    buf[31] = '\0';

    int r = consume(buf);      /* ownership transferred: consume() frees buf */
    free(buf);                 /* caller frees it too -> double free */
    return r;
}

int main(int argc, char **argv) {
    if (argc > 1) printf("%d\n", vuln(argv[1]));
    return 0;
}
