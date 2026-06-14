/* angr / pypcode pseudo-C  —  cl /Od */

void vuln(long long a0)
{
    char v0;            // [rsp+0x20]   (buf)

    sprintf(&v0, "%s", a0);
    return;
}
