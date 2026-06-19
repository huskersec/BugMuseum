/* angr / pypcode pseudo-C  —  cl /Od
 * The read plainly follows the free. */

int vuln(long long a0)
{
    char *buf;

    buf = malloc(32);
    strncpy(buf, a0, 31);
    buf[31] = 0;
    free(buf);                       // lifetime ends
    return (unsigned char)buf[0];    // use-after-free read
}
