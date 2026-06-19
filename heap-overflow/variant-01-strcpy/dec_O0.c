/* angr / pypcode pseudo-C  —  cl /Od
 * Destination is a heap allocation; copy has no bound. */

void vuln(long long a0)
{
    void *buf;            // [rsp+0x28]

    buf = malloc(16);
    strcpy(buf, a0);
    free(buf);
    return;
}
