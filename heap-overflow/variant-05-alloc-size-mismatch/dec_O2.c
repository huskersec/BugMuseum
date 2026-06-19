/* angr / pypcode pseudo-C  —  cl /O2 */

void vuln(long long a0)
{
    void              *buf;
    unsigned long long n;

    n   = strlen(a0);
    buf = malloc(n);            // no "+ 1" — allocation is one byte short
    strcpy(buf, a0);
    free(buf);
    return;
}
