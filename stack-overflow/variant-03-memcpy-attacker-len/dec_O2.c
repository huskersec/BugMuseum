/* angr / pypcode pseudo-C  —  cl /O2 */

void vuln(long long a0, unsigned int a1)
{
    char v0;            // [rsp+0x20]

    memcpy(&v0, a0, a1);   // no comparison of a1 against 16 anywhere in the body
    return;
}
