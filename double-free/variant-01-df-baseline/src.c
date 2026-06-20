#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Double-free (baseline) — the reference point.
 * buf is freed, then freed again. The first free ends the object's lifetime and
 * returns the chunk to the allocator's freelist. The second free operates on
 * memory the allocator now owns: it pushes the same chunk onto the freelist a
 * SECOND time. Unlike a use-after-free (which corrupts USER data — whatever
 * reused the chunk), a double-free corrupts the ALLOCATOR's own bookkeeping, so
 * the damage is heap-wide, not local. The release CRT often does this silently;
 * the debug CRT asserts; Page Heap / the Windows segment heap may raise
 * STATUS_HEAP_CORRUPTION (fast-fail). The classic exploit primitive: a chunk on
 * the freelist twice can later be handed out twice (see variant 08). */
int vuln(const char *input) {
    char *buf = (char *)malloc(32);
    strncpy(buf, input, 31);
    buf[31] = '\0';
    int r = (int)(unsigned char)buf[0];   /* a legitimate use, before any free */
    free(buf);                            /* lifetime ends; chunk to freelist */
    free(buf);                            /* freed again -> freelist corruption */
    return r;
}

int main(int argc, char **argv) {
    if (argc > 1) printf("%d\n", vuln(argv[1]));
    return 0;
}
