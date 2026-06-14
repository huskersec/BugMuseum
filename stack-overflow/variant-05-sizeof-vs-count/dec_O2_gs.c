/* angr / pypcode pseudo-C  —  cl /O2 /GS */

extern unsigned long long __security_cookie;

void vuln(long long a0)
{
    wchar_t v0;               // [rsp+0x20]
    unsigned long long v1;    // [rsp+0x40]   canary (directly above 32-byte buf)

    v1 = __security_cookie ^ (unsigned long long)&v0;
    wcsncpy(&v0, a0, 32);
    __security_check_cookie(v1 ^ (unsigned long long)&v0);
    return;
}
