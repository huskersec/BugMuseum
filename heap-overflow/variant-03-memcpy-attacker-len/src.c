#include <string.h>
#include <stdlib.h>

/* Axis B variant: the copy IS bounded — by a length the attacker supplies.
 * memcpy has a size argument, so the code *looks* disciplined. The flaw is
 * that `len` is tainted and never clamped to the 16-byte allocation. This is
 * the "looks-correct guard" shape: a present bound that trusts the wrong
 * source, now overflowing a heap chunk instead of a stack frame. */
void vuln(const char *input, unsigned int len) {
    char *buf = (char *)malloc(16);
    memcpy(buf, input, len);   // no clamp to the allocation size
    free(buf);
}

int main(int argc, char **argv) {
    if (argc > 2) vuln(argv[1], (unsigned int)strtoul(argv[2], 0, 10));
    return 0;
}
