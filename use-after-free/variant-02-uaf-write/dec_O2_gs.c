/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * No __security_cookie — identical to /O2. */

void vuln(long long a0)
{
    char *buf;

    buf = malloc(32);
    free(buf);
    strncpy(buf, a0, 31);
    buf[31] = 0;
    return;
}
