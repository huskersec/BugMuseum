/* angr / pypcode pseudo-C  —  cl /Od */

void vuln(long long a0)
{
    void *buf;                 // [rsp+0x28]
    unsigned long long n;

    buf = malloc(16);
    n = strlen(a0);
    strncpy(buf, a0, n);       // count = length of the source
    free(buf);
    return;
}
