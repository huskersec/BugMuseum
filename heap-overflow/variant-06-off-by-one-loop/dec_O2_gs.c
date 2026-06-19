/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * No __security_cookie — heap destination, nothing on the stack to guard. */

void vuln(long long a0)
{
    void *buf;

    buf = malloc(16);
    *(__int128 *)buf = *(__int128 *)a0;             // bytes 0..15
    *((char *)buf + 16) = *(char *)(a0 + 16);       // byte 16 (OOB)
    free(buf);
    return;
}
