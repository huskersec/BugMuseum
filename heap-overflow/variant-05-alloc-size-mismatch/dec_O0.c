/* angr / pypcode pseudo-C  —  cl /Od */

void vuln(long long a0)
{
    void              *buf;     // [rsp+0x30]
    unsigned long long n;       // [rsp+0x28]

    n   = strlen(a0);
    buf = malloc(n);            // missing + 1 for the terminator
    strcpy(buf, a0);            // writes n + 1 bytes
    free(buf);
    return;
}
