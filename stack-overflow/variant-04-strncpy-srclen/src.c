#include <string.h>

/* Axis B variant: a bounded-LOOKING call with the wrong bound.
 * strncpy takes a count, so it reads as the "safe" sibling of strcpy.
 * But the count here is derived from the SOURCE (strlen(input)), not the
 * destination size. For any input longer than 15 bytes the count exceeds
 * sizeof(buf), and strncpy writes past buf — it is strcpy with extra steps.
 * The 'n' lulls the reviewer; the third argument betrays it. */
void vuln(const char *input) {
    char buf[16];
    strncpy(buf, input, strlen(input));   // bound taken from src, not sizeof(buf)
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
