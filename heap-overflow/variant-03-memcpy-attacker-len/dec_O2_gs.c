/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * No __security_cookie — heap destination, nothing on the stack to guard. */

void vuln(long long a0, unsigned int a1)
{
    void *buf;

    buf = malloc(16);
    memcpy(buf, a0, a1);
    free(buf);
    return;
}
