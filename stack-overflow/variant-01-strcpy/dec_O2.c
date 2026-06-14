/* angr / pypcode pseudo-C  —  cl /O2
 * Identical body; frame math gone, local is just an rsp offset. */

void vuln(long long a0)
{
    char v0;            // [rsp+0x20]

    strcpy(&v0, a0);
    return;
}
