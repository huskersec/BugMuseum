/* angr / pypcode pseudo-C  —  cl /O2 /GS
 * No __security_cookie — identical to /O2. */

struct Buffer { char *data; unsigned long long len; };

void vuln(long long a0)
{
    struct Buffer *b;
    char          *p;

    b = malloc(16);
    b->data = malloc(32);
    p = b->data;
    strncpy(b->data, a0, 31);
    b->data[31] = 0;
    b->len = strlen(b->data);
    free(b->data);
    free(b);
    printf("%s\n", p);
    return;
}
