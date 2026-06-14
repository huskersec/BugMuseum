/* angr / pypcode pseudo-C  —  cl /O2 /GS */

extern unsigned long long __security_cookie;

void vuln(long long a0)
{
    char v0[16];              // [rsp+0x00]
    unsigned long long v1;    // [rsp+0x10]   canary (immediately above buf)

    v1 = __security_cookie ^ (unsigned long long)&v0;
    *(__int128 *)v0 = *(__int128 *)a0;        // bytes 0..15
    *((char *)&v1) = *(char *)(a0 + 16);      // byte 16 lands on the canary's low byte
    __security_check_cookie(v1 ^ (unsigned long long)&v0);
    return;
}
