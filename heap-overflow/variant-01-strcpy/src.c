#include <string.h>
#include <stdlib.h>

/* Classic heap buffer overflow.
 * buf is a 16-byte heap allocation; strcpy copies until the NUL in `input`
 * with no regard for the destination size. An input longer than 15 bytes
 * (+NUL) overruns buf and walks forward over adjacent heap data and the
 * allocator's bookkeeping for the next chunk.
 *
 * Unlike a stack smash (which trips on return), the crash here is deferred:
 * it usually surfaces later — at free() or a subsequent allocation — when the
 * heap manager notices its metadata is corrupt. Run under Full Page Heap
 * (gflags /p /enable vuln.exe /full) to fault immediately at the bad write. */
void vuln(const char *input) {
    char *buf = (char *)malloc(16);
    strcpy(buf, input);
    free(buf);
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
