#include <wchar.h>

/* Axis B variant: a bound that USES sizeof — and is still wrong.
 * This is the strongest disguise in the exhibit, because the code makes
 * exactly the gesture we tell people to make: it bounds the copy with
 * sizeof(buf). The defect is a units mismatch.
 *
 *   buf holds 16 wide chars  ->  sizeof(buf) == 32 BYTES (wchar_t is 2 bytes on Windows)
 *   wcsncpy counts in wchar_t UNITS, not bytes
 *
 * So the "bound" permits up to 32 wide chars = 64 bytes to be written into
 * a 32-byte buffer: a 2x overflow. On Windows this is the classic wide-string
 * overrun. The correct bound is _countof(buf) == sizeof(buf)/sizeof(buf[0]) == 16. */
void vuln(const wchar_t *input) {
    wchar_t buf[16];
    wcsncpy(buf, input, sizeof(buf));   // BUG: bytes (32) where a count (16) is required
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    return 0;
}
