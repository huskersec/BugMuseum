/* angr / pypcode pseudo-C  —  cl /O2 */

void vuln(long long a0)
{
    wchar_t v0;         // [rsp+0x20]

    wcsncpy(&v0, a0, 32);   // the "32" looks like a clean bound; it is sizeof(buf) in bytes, not _countof
    return;
}
