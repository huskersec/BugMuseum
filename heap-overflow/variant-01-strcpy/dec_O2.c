/* angr / pypcode pseudo-C  —  cl /O2
 * Identical body; the malloc'd pointer is the copy destination. */

void vuln(long long a0)
{
    void *buf;

    buf = malloc(16);
    strcpy(buf, a0);
    free(buf);
    return;
}
