#include <stdlib.h>

/* Axis B variant: a hand-rolled bounded copy with an off-by-one, on the heap.
 * No library call to pattern-match on. The loop even references the buffer
 * size (16), so it reads as a careful, size-aware copy. The single defect is
 * the comparison predicate: `<=` lets the index reach 16, writing buf[16] —
 * one byte past the 16-byte heap allocation. That lone byte lands on the next
 * chunk's header: the classic heap off-by-one. */
void vuln(const char *input) {
    char *buf = (char *)malloc(16);
    int i;
    for (i = 0; i <= 16; i++)   /* BUG: should be i < 16 */
        buf[i] = input[i];
    free(buf);
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
