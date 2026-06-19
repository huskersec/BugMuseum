/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * No __security_cookie — nothing on the stack to guard (heap destination). */

void vuln(long long a0)
{
    void *buf;

    buf = malloc(16);
    sprintf(buf, "%s", a0);
    free(buf);
    return;
}
