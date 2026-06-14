/* angr / pypcode pseudo-C  —  cl /Od */

void vuln(long long a0)
{
    wchar_t v0;         // [rsp+0x20]   buf[16] (32 bytes)

    wcsncpy(&v0, a0, 32);   // count 32 in wchar_t units; buf holds only 16 -> 2x overflow
    return;
}
