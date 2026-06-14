/* angr / pypcode pseudo-C  —  cl /O2 /GS */

extern unsigned long long __security_cookie;

void vuln(long long a0, unsigned int a1)
{
    char v0;                  // [rsp+0x20]
    unsigned long long v1;    // [rsp+0x28]   canary

    v1 = __security_cookie ^ (unsigned long long)&v0;
    memcpy(&v0, a0, a1);
    __security_check_cookie(v1 ^ (unsigned long long)&v0);
    return;
}
