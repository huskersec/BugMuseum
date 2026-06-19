/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * No __security_cookie — heap destination, nothing on the stack to guard. */

void vuln(long long a0)
{
    void              *buf;
    unsigned long long n;

    n   = strlen(a0);
    buf = malloc(n);
    strcpy(buf, a0);
    free(buf);
    return;
}
