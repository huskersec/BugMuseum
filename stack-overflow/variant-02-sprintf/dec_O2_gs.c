/* angr / pypcode pseudo-C  —  cl /O2 /GS */

extern unsigned long long __security_cookie;

void vuln(long long a0)
{
    char v0;                  // [rsp+0x20]
    unsigned long long v1;    // [rsp+0x28]   canary

    v1 = __security_cookie ^ (unsigned long long)&v0;
    sprintf(&v0, "%s", a0);
    __security_check_cookie(v1 ^ (unsigned long long)&v0);
    return;
}
