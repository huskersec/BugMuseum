/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * The canary appears explicitly: cookie XOR rsp, stored above buf, verified via a call.
 * Contrast mingw-reference/dec_O2_gs.c, which shows the GCC __stack_chk_guard compare. */

extern unsigned long long __security_cookie;

void vuln(long long a0)
{
    char v0;                  // [rsp+0x20]
    unsigned long long v1;    // [rsp+0x28]   canary slot

    v1 = __security_cookie ^ (unsigned long long)&v0;   // cookie ^ rsp
    strcpy(&v0, a0);
    __security_check_cookie(v1 ^ (unsigned long long)&v0);
    return;
}
