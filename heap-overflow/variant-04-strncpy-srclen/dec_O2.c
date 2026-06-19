/* angr / pypcode pseudo-C  —  cl /O2 */

void vuln(long long a0)
{
    void *buf;
    unsigned long long n;

    buf = malloc(16);
    n = strlen(a0);
    strncpy(buf, a0, n);       // no 16 / allocation size anywhere — count tracks the source
    free(buf);
    return;
}
