#include <string.h>
#include <stdlib.h>

/* Axis B variant: the copy IS bounded — by a length the attacker supplies.
 * memcpy has a size argument, so the code *looks* disciplined. The flaw is
 * that `len` is tainted: it governs how many bytes land in buf[16] with no
 * check against the destination size. This is the "looks-correct guard"
 * shape in miniature — the bound exists, it just trusts the wrong source. */
void vuln(const char *input, unsigned int len) {
    char buf[16];
    memcpy(buf, input, len);   // no clamp to sizeof(buf)
}

int main(int argc, char **argv) {
    if (argc > 2) vuln(argv[1], (unsigned int)strtoul(argv[2], 0, 10));
    return 0;
}
