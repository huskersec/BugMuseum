/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * No __security_cookie — heap destination, nothing on the stack to guard. */

void vuln(long long a0)
{
    void *buf;
    unsigned long long n;

    buf = malloc(16);
    n = strlen(a0);
    strncpy(buf, a0, n);
    free(buf);
    return;
}
