/* angr / pypcode pseudo-C  —  cl /Od */

void vuln(long long a0)
{
    char v0[16];        // [rsp+0x00]   (buf)
    int i;

    for (i = 0; i <= 16; i++)          // <= reaches index 16
        v0[i] = *(char *)(a0 + i);
    return;
}
