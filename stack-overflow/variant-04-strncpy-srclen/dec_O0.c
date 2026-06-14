/* angr / pypcode pseudo-C  —  cl /Od */

void vuln(long long a0)
{
    char v0;                  // [rsp+0x20]   (buf)
    unsigned long long n;

    n = strlen(a0);
    strncpy(&v0, a0, n);      // count = length of the source
    return;
}
