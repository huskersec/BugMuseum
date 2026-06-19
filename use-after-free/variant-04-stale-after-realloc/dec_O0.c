/* angr / pypcode pseudo-C  —  cl /Od */

void vuln(long long a0)   /* a0 = more */
{
    char *buf;
    char *grown;

    buf = malloc(16);
    strcpy(buf, "id=");
    grown = realloc(buf, 16 + strlen(a0) + 1);
    if (!grown) { free(buf); return; }
    strcat(buf, a0);          // BUG: uses the realloc INPUT (stale if relocated), not grown
    printf("%s\n", grown);
    return;
}
