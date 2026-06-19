/* angr / pypcode pseudo-C  —  cl /O2 */

void vuln(long long a0, unsigned int a1)
{
    void *buf;

    buf = malloc(16);
    memcpy(buf, a0, a1);   // no comparison of a1 against 16 anywhere in the body
    free(buf);
    return;
}
