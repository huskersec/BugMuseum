/* Axis B variant: a hand-rolled bounded copy with an off-by-one.
 * No library call to pattern-match on at all. The loop even references
 * the buffer size (16), so it reads as a careful, size-aware copy. The
 * single defect is the comparison predicate: `<=` lets the index reach
 * 16, writing buf[16] — one byte past the end. That lone byte is the
 * classic off-by-one / "poison null byte" smash of whatever sits
 * immediately above the buffer (saved register LSB, canary byte, ...). */
void vuln(const char *input) {
    char buf[16];
    int i;
    for (i = 0; i <= 16; i++)   /* BUG: should be i < 16 */
        buf[i] = input[i];
}

int main(int argc, char **argv) {
    if (argc > 1) vuln(argv[1]);
    return 0;
}
