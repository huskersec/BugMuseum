/* angr / pypcode pseudo-C  —  cl /O2 */

void vuln(long long a0)
{
    char v0;            // [rsp+0x20]

    sprintf(&v0, "%s", a0);   // may render as strcpy(&v0, a0) if the build folded "%s"
    return;
}
