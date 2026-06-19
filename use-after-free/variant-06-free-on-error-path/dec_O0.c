/* angr / pypcode pseudo-C  —  cl /Od */

int vuln(long long a0)
{
    char *buf;
    int   rc = 0;

    buf = malloc(32);
    strncpy(buf, a0, 31);
    buf[31] = 0;
    if (buf[0] != 0)
        rc = (int)strlen(buf);
    free(buf);                                  // cleanup
    if (rc == 0)
        printf("rejected: '%s'\n", buf);        // use-after-free on the reject path
    return rc;
}
