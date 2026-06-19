/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * Note: NO __security_cookie appears. /GS guards stack arrays; vuln has none
 * (the overflowed buffer is heap-allocated), so the hardened build is identical
 * to /O2. Contrast the stack exhibit, where /GS injects a cookie. */

void vuln(long long a0)
{
    void *buf;

    buf = malloc(16);
    strcpy(buf, a0);
    free(buf);
    return;
}
