/* angr / pypcode pseudo-C  —  cl /Od */

char *g_last;

void vuln(long long a0)
{
    char *buf;

    buf = malloc(32);
    strncpy(buf, a0, 31);
    buf[31] = 0;
    g_last = buf;                 // retain an alias
    free(buf);                    // freed via buf
    printf("%s\n", g_last);       // used via the dangling alias
    return;
}
