/* angr / pypcode pseudo-C  —  cl /Od */

void vuln(long long a0, unsigned int a1)
{
    char v0;            // [rsp+0x20]   (buf)

    memcpy(&v0, a0, a1);   // size is the incoming parameter — never clamped to 16
    return;
}
