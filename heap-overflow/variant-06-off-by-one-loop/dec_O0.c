/* angr / pypcode pseudo-C  —  cl /Od */

void vuln(long long a0)
{
    void *buf;            // [rsp+0x28]
    int   i;

    buf = malloc(16);
    for (i = 0; i <= 16; i++)                       // <= reaches index 16
        *((char *)buf + i) = *(char *)(a0 + i);
    free(buf);
    return;
}
