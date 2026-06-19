/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * No __security_cookie — identical to /O2. */

void vuln(long long a0)
{
    char *buf;
    char *grown;

    buf = malloc(16);
    strcpy(buf, "id=");
    grown = realloc(buf, 16 + strlen(a0) + 1);
    if (!grown) { free(buf); return; }
    strcat(buf, a0);
    printf("%s\n", grown);
    return;
}
