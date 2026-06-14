/* angr / pypcode pseudo-C  —  cl /Od
 * Dest is a clean stack local; copy has no bound. */

void vuln(long long a0)
{
    char v0;            // [rsp+0x20]   (buf)

    strcpy(&v0, a0);
    return;
}
