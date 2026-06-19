/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * No __security_cookie — UAF is not a stack-buffer bug; identical to /O2. */

int vuln(long long a0)
{
    char *buf;

    buf = malloc(32);
    strncpy(buf, a0, 31);
    buf[31] = 0;
    free(buf);
    return (unsigned char)buf[0];
}
