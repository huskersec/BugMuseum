/* angr / pypcode pseudo-C  —  cl /Od */

void vuln(long long a0)
{
    char *buf;

    buf = malloc(32);
    free(buf);                  // lifetime ends
    strncpy(buf, a0, 31);       // use-after-free write
    buf[31] = 0;                // (also a write into freed memory)
    return;
}
