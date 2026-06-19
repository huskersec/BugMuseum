/* angr / pypcode pseudo-C  —  cl /Od */

void vuln(long long a0, unsigned int a1)
{
    void *buf;            // [rsp+0x28]

    buf = malloc(16);
    memcpy(buf, a0, a1);   // size is the incoming parameter — never clamped to 16
    free(buf);
    return;
}
