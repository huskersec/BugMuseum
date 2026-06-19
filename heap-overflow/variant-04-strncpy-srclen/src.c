#include <string.h>
#include <stdlib.h>

/* Axis B variant: a bounded-LOOKING call with the wrong bound, on the heap.
 * strncpy takes a count, so it reads as the "safe" sibling of strcpy. But the
 * count here is derived from the SOURCE (strlen(input)), not the 16-byte
 * allocation. For any input longer than 15 bytes the count exceeds the chunk,
 * and strncpy overruns it — it is strcpy with extra steps. */
void vuln(const char *input) {
    char *buf = (char *)malloc(16);
    strncpy(buf, input, strlen(input));   // bound taken from src, not the alloc size
    free(buf);
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
