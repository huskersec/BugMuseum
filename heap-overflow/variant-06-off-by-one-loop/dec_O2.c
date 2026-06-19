/* angr / pypcode pseudo-C  —  cl /O2
 * Loop gone; the off-by-one is a 16-byte block move plus one trailing byte. */

void vuln(long long a0)
{
    void *buf;

    buf = malloc(16);
    *(__int128 *)buf = *(__int128 *)a0;             // bytes 0..15
    *((char *)buf + 16) = *(char *)(a0 + 16);       // byte 16 — one past the 16-byte chunk
    free(buf);
    return;
}
