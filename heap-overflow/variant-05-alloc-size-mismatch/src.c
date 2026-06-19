#include <string.h>
#include <stdlib.h>

/* Axis B variant: the allocation is sized to the input — and is STILL wrong.
 * This is the strongest disguise, because the code does the responsible thing:
 * it allocates exactly as much as the data needs. The defect is arithmetic:
 * strlen() counts the characters but NOT the trailing NUL, so malloc(strlen)
 * is one byte short. strcpy then writes strlen(input)+1 bytes (the text plus
 * its terminator) into a buffer of strlen(input) bytes — a one-byte heap
 * overflow for ANY input length. The lone overwritten byte (a 0) landing on
 * the next chunk's header low byte is the classic, historically devastating
 * heap off-by-one. The fix is malloc(strlen(input) + 1). */
void vuln(const char *input) {
    size_t n = strlen(input);
    char *buf = (char *)malloc(n);    /* BUG: should be n + 1 */
    strcpy(buf, input);
    free(buf);
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
