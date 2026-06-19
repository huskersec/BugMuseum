/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * No __security_cookie — identical to /O2. */

char *g_last;

void vuln(long long a0)
{
    char *buf;

    buf = malloc(32);
    strncpy(buf, a0, 31);
    buf[31] = 0;
    g_last = buf;
    free(buf);
    printf("%s\n", g_last);
    return;
}
