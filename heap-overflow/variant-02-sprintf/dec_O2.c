/* angr / pypcode pseudo-C  —  cl /O2 */

void vuln(long long a0)
{
    void *buf;

    buf = malloc(16);
    sprintf(buf, "%s", a0);   // may render as strcpy(buf, a0) if the build folded "%s"
    free(buf);
    return;
}
