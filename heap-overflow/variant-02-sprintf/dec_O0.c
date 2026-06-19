/* angr / pypcode pseudo-C  —  cl /Od */

void vuln(long long a0)
{
    void *buf;            // [rsp+0x28]

    buf = malloc(16);
    sprintf(buf, "%s", a0);
    free(buf);
    return;
}
