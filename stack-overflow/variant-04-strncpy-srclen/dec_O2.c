/* angr / pypcode pseudo-C  —  cl /O2 */

void vuln(long long a0)
{
    char v0;                  // [rsp+0x20]
    unsigned long long n;

    n = strlen(a0);
    strncpy(&v0, a0, n);      // no 16 / sizeof anywhere — count tracks the source
    return;
}
